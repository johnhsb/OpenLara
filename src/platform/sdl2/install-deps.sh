#!/bin/sh
# Checks the libraries that OpenLara (next to this script) needs and installs the missing ones
# with the package manager of the distribution (apt, dnf, zypper or pacman).
#
#   ./install-deps.sh           checks, shows the install command and asks before it runs it
#   ./install-deps.sh --check   only checks: exit 0 when all is there, 1 when a library is missing,
#                               2 when the system can't run this build (glibc too old, musl...)
#   ./install-deps.sh --yes     installs without asking
#
# The libraries: SDL2, EGL and GLES (the graphics driver of the system), ALSA (sound), udev (gamepads),
# libstdc++ and glibc 2.34 or newer (Ubuntu 22.04, Debian 12, Fedora 35, Raspberry Pi OS 12 and newer).

MODE=ask
case "$1" in
    --check) MODE=check ;;
    --yes|-y) MODE=yes ;;
    "") ;;
    *) sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
esac

HERE=$(cd "$(dirname "$0")" && pwd)
BIN="$HERE/OpenLara"
LIBS="libSDL2-2.0.so.0 libEGL.so.1 libGLESv2.so.2 libasound.so.2 libudev.so.1 libstdc++.so.6 libgcc_s.so.1"

if [ "$(uname -s)" != Linux ]; then
    echo "This is the Linux build, this system is $(uname -s)."
    exit 2
fi

# the missing libraries, from ldd of the game when it is there, else from the cache of the loader
if [ -x "$BIN" ] && command -v ldd >/dev/null 2>&1; then
    OUT=$(ldd "$BIN" 2>&1)
    if echo "$OUT" | grep -q "version .GLIBC"; then # GLIBC_2.34 or GLIBCXX_3.4.30 of libstdc++
        NEED=$(echo "$OUT" | grep -o "GLIBC[A-Z]*_[0-9.]*" | sort -u | tr '\n' ' ')
        HAVE=$(ldd --version 2>&1 | head -n 1 | grep -o "[0-9][0-9.]*$")
        echo "This build needs $NEED, the system is older (glibc $HAVE)."
        echo "A package can't change that: use a newer distribution (Ubuntu 22.04, Debian 12 or newer),"
        echo "or build OpenLara on this system (src/platform/sdl2/build.sh)."
        exit 2
    fi
    if echo "$OUT" | grep -qi "not a dynamic executable\|musl"; then
        echo "This build is for glibc systems, it can't run here (musl, e.g. Alpine): build it on this system."
        exit 2
    fi
    MISSING=$(echo "$OUT" | grep "=> not found" | sed 's/^[[:space:]]*\([^[:space:]]*\).*/\1/' | sort -u | tr '\n' ' ')
else
    LDCONFIG=$(command -v ldconfig || echo /sbin/ldconfig)
    CACHE=$("$LDCONFIG" -p 2>/dev/null)
    MISSING=
    for lib in $LIBS; do
        echo "$CACHE" | grep -q "^[[:space:]]*$lib " || MISSING="$MISSING$lib "
    done
fi

if [ -z "$MISSING" ]; then
    echo "All the libraries OpenLara needs are installed."
    exit 0
fi
echo "Missing: $MISSING"

# the package of a library for each package manager
apt_pkg() {
    case "$1" in
        libSDL2-2.0.so.0) echo libsdl2-2.0-0 ;;
        libEGL.so.1)      echo libegl1 ;;
        libGLESv2.so.2)   echo libgles2 ;;
        libasound.so.2)   # renamed for the 64-bit time_t of Ubuntu 24.04 and Debian 13 (glibc 2.39 and newer)
            if apt-cache show libasound2t64 >/dev/null 2>&1; then echo libasound2t64
            elif apt-cache show libasound2 >/dev/null 2>&1; then echo libasound2
            else # no package lists yet
                v=$(ldd --version 2>&1 | head -n 1 | grep -o "[0-9][0-9.]*$" | cut -d. -f2)
                if [ "${v:-0}" -ge 39 ]; then echo libasound2t64; else echo libasound2; fi
            fi ;;
        libudev.so.1)     echo libudev1 ;;
        libstdc++.so.6)   echo libstdc++6 ;;
        libgcc_s.so.1)    echo libgcc-s1 ;;
    esac
}
pacman_pkg() {
    case "$1" in
        libSDL2-2.0.so.0)            echo sdl2 ;;
        libEGL.so.1|libGLESv2.so.2)  echo libglvnd ;;
        libasound.so.2)              echo alsa-lib ;;
        libudev.so.1)                echo systemd-libs ;;
        libstdc++.so.6|libgcc_s.so.1) echo gcc-libs ;;
    esac
}
rpm_cap() { # dnf and zypper install a package by the library it gives
    case "$(uname -m)" in
        x86_64|aarch64|ppc64le|s390x) echo "$1()(64bit)" ;;
        *) echo "$1" ;;
    esac
}

SUDO=
[ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1 && SUDO=sudo

PKGS=
if command -v apt-get >/dev/null 2>&1; then
    for lib in $MISSING; do PKGS="$PKGS $(apt_pkg "$lib")"; done
    CMD="$SUDO apt-get install -y$PKGS"
    PRE="$SUDO apt-get update"
elif command -v dnf >/dev/null 2>&1; then
    for lib in $MISSING; do PKGS="$PKGS '$(rpm_cap "$lib")'"; done
    CMD="$SUDO dnf install -y$PKGS"
elif command -v zypper >/dev/null 2>&1; then
    for lib in $MISSING; do PKGS="$PKGS '$(rpm_cap "$lib")'"; done
    CMD="$SUDO zypper --non-interactive install$PKGS"
elif command -v pacman >/dev/null 2>&1; then
    for lib in $MISSING; do PKGS="$PKGS $(pacman_pkg "$lib")"; done
    PKGS=$(echo "$PKGS" | tr ' ' '\n' | sort -u | tr '\n' ' ')
    CMD="$SUDO pacman -Syu --needed --noconfirm $PKGS" # Arch installs on an up to date system
else
    echo "No known package manager (apt, dnf, zypper, pacman): install the packages that give these libraries."
    exit 1
fi

echo "Install command: ${PRE:+$PRE && }$CMD"
[ "$MODE" = check ] && exit 1

if [ "$MODE" = ask ]; then
    printf "Run it now? [y/N] "
    read -r answer
    case "$answer" in y|Y|yes|YES) ;; *) echo "Nothing installed."; exit 1 ;; esac
fi

[ -n "$PRE" ] && { eval "$PRE" || exit 1; }
eval "$CMD" || exit 1
exec "$0" --check
