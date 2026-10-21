#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$SCRIPT_DIR/OpenLara.xcodeproj"
BUILD_DIR="$SCRIPT_DIR/build"

CONFIG="Release"
ACTION="build"

usage() {
    echo "Usage: $0 [options]"
    echo "  -d, --debug     Debug 빌드 (기본값: Release)"
    echo "  -c, --clean     빌드 전 clean 실행"
    echo "  -r, --run       빌드 후 앱 실행"
    echo "  -h, --help      도움말"
    exit 0
}

RUN=0
CLEAN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--debug)   CONFIG="Debug"; shift ;;
        -c|--clean)   CLEAN=1; shift ;;
        -r|--run)     RUN=1; shift ;;
        -h|--help)    usage ;;
        *) echo "알 수 없는 옵션: $1"; usage ;;
    esac
done

# xcode-select 가 Command Line Tools 를 가리키면 xcodebuild 를 쓸 수 없으므로 Xcode.app 을 직접 지정
if ! xcodebuild -version >/dev/null 2>&1; then
    if [[ -d /Applications/Xcode.app/Contents/Developer ]]; then
        export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
    else
        echo "[ERROR] Xcode 를 찾을 수 없습니다. Xcode 를 설치하거나 xcode-select -s 로 경로를 지정하세요."
        exit 1
    fi
fi

echo "==> 빌드 설정: $CONFIG"
echo "==> 프로젝트: $PROJECT"
echo "==> 출력 디렉토리: $BUILD_DIR/$CONFIG"

if [[ $CLEAN -eq 1 ]]; then
    echo "==> clean 실행 중..."
    xcodebuild \
        -project "$PROJECT" \
        -scheme OpenLara \
        -configuration "$CONFIG" \
        -derivedDataPath "$BUILD_DIR" \
        clean
fi

echo "==> 빌드 중..."
BUILD_CMD=(xcodebuild
    -project "$PROJECT"
    -scheme OpenLara
    -configuration "$CONFIG"
    -derivedDataPath "$BUILD_DIR"
    MACOSX_DEPLOYMENT_TARGET=12.0
    build)

if command -v xcpretty >/dev/null 2>&1; then
    set -o pipefail
    "${BUILD_CMD[@]}" 2>&1 | xcpretty
else
    "${BUILD_CMD[@]}"
fi

APP_PATH="$BUILD_DIR/Build/Products/$CONFIG/OpenLara.app"

if [[ ! -d "$APP_PATH" ]]; then
    echo "[ERROR] OpenLara.app 를 찾을 수 없습니다."
    exit 1
fi

echo "==> 빌드 완료: $APP_PATH"

if [[ $RUN -eq 1 ]]; then
    echo "==> 앱 실행 중..."
    open "$APP_PATH"
fi
