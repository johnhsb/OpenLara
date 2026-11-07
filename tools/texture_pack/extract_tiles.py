#!/usr/bin/env python3
# writes the texture tiles of the TR1 PHD levels as 256x256 RGBA PNGs: <out dir>/<LEVEL>_<tile>.png
# (palette index 0 is transparent), the input of the upscaler. Python 3 only, no packages.
# usage: extract_tiles.py <TR1 data dir> <out dir>
import struct, sys, glob, os, zlib

def png(path, w, h, rgba):
    def chunk(t, data):
        c = t + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    raw = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

data_dir, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)
for path in sorted(glob.glob(os.path.join(data_dir, '*.PHD'))):
    d = open(path, 'rb').read(); o = 0
    def u16():
        global o; v = struct.unpack_from('<H', d, o)[0]; o += 2; return v
    def u32():
        global o; v = struct.unpack_from('<I', d, o)[0]; o += 4; return v
    u32(); nt = u32(); tiles = o; o += nt * 65536; u32()
    for r in range(u16()):   # rooms
        o += 16; n = u32(); o += n * 2; n = u16(); o += n * 32; z = u16(); x = u16(); o += z * x * 8
        o += 2; n = u16(); o += n * 18; n = u16(); o += n * 18; o += 4
    for size in (2, 2, 4, 32, 6, 8, 2, 4, 2, 18, 32, 20, 16, 8, 16, 16):   # floor data .. sound sources
        n = u32(); o += n * size
    boxes = u32(); o += boxes * 20; n = u32(); o += n * 2; o += boxes * 12   # boxes, overlaps, zones
    n = u32(); o += n * 2       # animated textures
    n = u32(); o += n * 22      # items
    o += 32 * 256               # light map
    pal = [tuple(min(255, c * 4) for c in d[o + i * 3:o + i * 3 + 3]) for i in range(256)]
    level = os.path.basename(path)[:-4]
    for t in range(nt):
        idx = d[tiles + t * 65536:tiles + (t + 1) * 65536]
        rgba = bytearray(65536 * 4)
        color = [pal[c] if c else None for c in idx]
        for i, c in enumerate(idx):
            if c:
                rgba[i * 4:i * 4 + 4] = bytes(pal[c]) + b'\xff'
    # the transparent texels take the color of the opaque ones next to them (still transparent), so the upscaler
    # doesn't darken the edges of the cut-outs with black
        for step in range(8):
            grow = {}
            for i in range(65536):
                if color[i] is not None: continue
                x, y = i & 255, i >> 8
                near = [color[j] for j in (i - 1 if x else -1, i + 1 if x < 255 else -1, i - 256 if y else -1, i + 256 if y < 255 else -1)
                        if j >= 0 and color[j] is not None]
                if near:
                    grow[i] = tuple(sum(c[k] for c in near) // len(near) for k in range(3))
            if not grow: break
            for i, c in grow.items():
                color[i] = c
                rgba[i * 4:i * 4 + 3] = bytes(c)
        png(os.path.join(out_dir, '%s_%d.png' % (level, t)), 256, 256, bytes(rgba))
    print(level, nt, 'tiles')
