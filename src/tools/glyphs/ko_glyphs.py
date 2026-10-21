#!/usr/bin/env python3
# Korean glyphs generator, a macOS/Linux take on glyphs/main.cpp for KO
#
# Encodes the UTF-8 strings between "#if 0" and "#endif" in src/lang/ko.h into
# glyph sequences, and updates src/lang/glyph_ko.bmp and src/lang/glyph_ko.h.
# Unlike main.cpp the glyph order is kept stable (src/lang/glyph_ko.txt): the
# glyphs already in the bitmap stay where they are, new ones are appended and
# rendered with the font given (Malgun Gothic matches the existing ones).
#
# usage: python3 ko_glyphs.py <lang dir> <font.ttf>
# requires Pillow

import os, re, struct, sys
from PIL import Image, ImageDraw, ImageFont

GLYPH_SIZE = 16

def glyph_code(i):
    # same as main.cpp, decoded by UI::getWideCharGlyph
    i += 1
    if i > 255:
        i += 1
    return i + 256

def is_reserved(i):
    # a code with a zero byte would end the C string
    return (glyph_code(i) & 0xFF) == 0

def load_order(path, block, old_text):
    if os.path.exists(path):
        return list(open(path, encoding='utf-8').read())
    # no order file yet, recover it from the strings encoded by main.cpp
    chars = [c for c in block if ord(c) > 255]
    enc = old_text[old_text.index('const char *STR_KO[]', old_text.index('#endif')):]
    codes = []
    for m in re.finditer(r'\\x11((?:\\x[0-9A-F]{2}\\x[0-9A-F]{2})+?)\\xFF\\xFF', enc):
        codes += [int(h, 16) * 256 + int(l, 16) for h, l in re.findall(r'\\x([0-9A-F]{2})\\x([0-9A-F]{2})', m.group(1))]
    if len(codes) != len(chars):
        sys.exit('the encoded strings don\'t match the source ones, can\'t recover the glyph order')
    decode = {glyph_code(i): i for i in range(4096)}
    order = {}
    for c, code in zip(chars, codes):
        order[decode[code]] = c
    return [order.get(i, '\0') for i in range(max(order) + 1)]

def render(font, ch):
    im = Image.new('L', (GLYPH_SIZE, GLYPH_SIZE), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = '1'
    d.text((0, -5), ch, font=font, fill=255)
    # semi-bold like GDI FW_SEMIBOLD
    bold = Image.new('L', im.size, 0)
    bold.paste(im, (1, 0))
    return Image.composite(bold, im, bold)

def glyph_width(cell):
    px = cell.load()
    cols = [x for x in range(GLYPH_SIZE) if any(px[x, y] > 127 for y in range(GLYPH_SIZE))]
    return max(cols) + 2 if cols else GLYPH_SIZE

def encode(block, index):
    out = []
    seq = False
    for i, c in enumerate(block):
        if ord(c) > 255:
            if not seq:
                out.append('\\x11')
                seq = True
            code = glyph_code(index[c])
            out.append('\\x%02X\\x%02X' % (code >> 8, code & 0xFF))
        else:
            if seq:
                out.append('\\xFF\\xFF')
                if c != '"':
                    out.append('""')
                seq = False
            out.append(c)
    return ''.join(out)

def save_bmp1(img, path):
    # 1 bpp bottom-up BMP with a black/white palette, the same header as main.cpp writes
    w, h = img.size
    px = img.load()
    stride = w // 8
    bits = bytearray(stride * h)
    for y in range(h):
        row = (h - 1 - y) * stride
        for x in range(w):
            if px[x, y] > 127:
                bits[row + x // 8] |= 0x80 >> (x % 8)
    offset = 14 + 40 + 8
    hdr = struct.pack('<2sIHHI', b'BM', offset + len(bits), 0, 0, offset)
    hdr += struct.pack('<IiiHHIIiiII', 40, w, h, 1, 1, 0, len(bits), 0, 0, 0, 0)
    hdr += bytes([0, 0, 0, 0, 255, 255, 255, 0])
    data = hdr + bytes(bits)
    open(path, 'wb').write(data)
    return data

def bin2c(data, name):
    lines = ['#ifndef __%s__' % name, '#define __%s__' % name, '',
             'static unsigned int size_%s = %d;' % (name, len(data)),
             'const unsigned char %s[] = {' % name]
    for i in range(0, len(data), 16):
        chunk = ', '.join('0x%02X' % b for b in data[i:i + 16])
        lines.append('    ' + chunk + (',' if i + 16 < len(data) else ''))
    lines += ['};', '', '#endif']
    return '\n'.join(lines)

def main():
    if len(sys.argv) != 3:
        sys.exit('usage: ko_glyphs.py <lang dir> <font.ttf>')
    lang_dir, font_path = sys.argv[1], sys.argv[2]
    src_path   = os.path.join(lang_dir, 'ko.h')
    order_path = os.path.join(lang_dir, 'glyph_ko.txt')
    bmp_path   = os.path.join(lang_dir, 'glyph_ko.bmp')
    hdr_path   = os.path.join(lang_dir, 'glyph_ko.h')

    raw  = open(src_path, 'rb').read()
    bom  = raw.startswith(b'\xef\xbb\xbf')
    text = raw.decode('utf-8-sig').replace('\r\n', '\n')

    start = text.index('#if 0') + len('#if 0')
    end   = text.index('#endif', start)
    block = text[start:end]

    order = load_order(order_path, block, text)
    old_count = len(order)

    for c in block:
        if ord(c) > 255 and c not in order:
            while is_reserved(len(order)):
                order.append('\0')
            order.append(c)
    index = {c: i for i, c in enumerate(order) if c != '\0'}

    # glyphs bitmap, 16 x 16 cells, 16 glyphs per row
    old = Image.open(bmp_path).convert('L') if os.path.exists(bmp_path) else None
    font = ImageFont.truetype(font_path, GLYPH_SIZE)
    rows = (len(order) + 15) // 16
    bmp = Image.new('L', (256, max(rows, 1) * GLYPH_SIZE), 0)
    widths = []
    for i, c in enumerate(order):
        x, y = (i % 16) * GLYPH_SIZE, (i // 16) * GLYPH_SIZE
        if c == '\0':
            cell = Image.new('L', (GLYPH_SIZE, GLYPH_SIZE), 0)
        elif old is not None and i < old_count and y + GLYPH_SIZE <= old.height:
            cell = old.crop((x, y, x + GLYPH_SIZE, y + GLYPH_SIZE))
        else:
            cell = render(font, c)
        bmp.paste(cell, (x, y))
        widths.append(glyph_width(cell))
    data = save_bmp1(bmp, bmp_path)
    open(hdr_path, 'w', newline='\r\n').write(bin2c(data, 'GLYPH_KO'))

    # source file: keep everything up to #endif, then the generated part
    gen = ['', '', '#define KO_GLYPH_COUNT %d' % len(order), '#define KO_GLYPH_BASE 0', 'const uint8 KO_GLYPH_WIDTH[] = {']
    for i in range(0, len(widths), 16):
        gen.append('    ' + ''.join('%d, ' % w for w in widths[i:i + 16]))
    gen[-1] += '};'
    out = text[:end + len('#endif')] + '\n'.join(gen) + '\n' + encode(block, index) + '\n#endif\n'
    data = out.replace('\n', '\r\n').encode('utf-8')
    open(src_path, 'wb').write((b'\xef\xbb\xbf' if bom else b'') + data)
    open(order_path, 'w', encoding='utf-8').write(''.join(order))

    print('glyphs: %d (%d new)' % (len(order), len(order) - old_count))

if __name__ == '__main__':
    main()
