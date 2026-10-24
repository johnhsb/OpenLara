#!/usr/bin/env python3
# Local server of the subtitle sync editor (subtitle_sync.html)
#
# Serves the editor with the TR2 cutscene subtitles read from the sources, the
# voices from the music folder of the game, and writes the edited lines back:
# the /* CUTn */ block of src/lang.h (STR_TR2_SUBS) or of the UTF-8 strings of
# src/lang/ko.h, then for Korean runs tools/glyphs/ko_glyphs.py to regenerate
# the glyph encoded strings the game is built with, and builds on request.
#
# usage: python3 serve.py [--music <game>/music] [--font <font.ttf>] [--port 8765] [--src <src dir>]
# ko_glyphs.py requires Pillow

import argparse, json, os, re, subprocess, sys, threading, webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = os.path.join(HERE, 'subtitle_sync.html')
FONTS = [  # Malgun Gothic matches the glyphs already in the bitmap
    'C:/Windows/Fonts/malgun.ttf',
    '/Library/Fonts/malgun.ttf',
    os.path.expanduser('~/Library/Fonts/malgun.ttf'),
    '/usr/share/fonts/truetype/malgun/malgun.ttf',
    '/System/Library/Fonts/AppleSDGothicNeo.ttc',
    '/usr/share/fonts/truetype/nanum/NanumGothic.ttf',
]

args = None
lock = threading.Lock()


def lang_file(lang):
    return os.path.join(args.src, 'lang.h' if lang == 'en' else os.path.join('lang', 'ko.h'))


def read_text(path):
    raw = open(path, 'rb').read()
    bom = raw.startswith(b'\xef\xbb\xbf')
    text = raw[3 if bom else 0:].decode('utf-8', 'surrogateescape')  # lang.h holds some bytes of other encodings, kept as they are
    eol = '\r\n' if '\r\n' in text else '\n'
    return text.replace('\r\n', '\n'), bom, eol


def write_text(path, text, bom, eol):
    data = text.replace('\n', eol).encode('utf-8', 'surrogateescape')
    open(path, 'wb').write((b'\xef\xbb\xbf' if bom else b'') + data)


def section(text, lang):
    # the range of the text holding the TR2 cutscene subtitles
    if lang == 'en':
        start = text.index('#define STR_TR2_SUBS')
        end = text.index('\n\n', start)
    else:  # the UTF-8 strings of ko.h, the glyph encoded ones after "#endif" are generated from them
        a = text.index('#if 0')
        start = text.index('// TR2 subtit', a)
        end = text.index('// TR1 levels', start)
    return start, end


def blocks(text, lang):
    # {CUTn: (first line, end line, lines)} of the strings of each cutscene
    start, end = section(text, lang)
    lines = text.split('\n')
    first = text.count('\n', 0, start)
    last = text.count('\n', 0, end)
    out = {}
    i = first
    while i <= last:
        m = re.search(r'/\*\s*(CUT\d+)\s*\*/', lines[i])
        if m:
            j = i + 1
            while j < len(lines) and lines[j].strip().startswith('"'):
                j += 1
            out[m.group(1)] = (i + 1, j, lines[i + 1:j])
            i = j
        else:
            i += 1
    return out


def subs(lang):
    text, _, _ = read_text(lang_file(lang))
    res = []
    for name, (_, _, lines) in blocks(text, lang).items():
        raw = ''.join(''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', l)) for l in lines)
        res.append({'name': name, 'raw': raw})
    return res


def escape(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')


def apply(lang, cut, cues):
    if lang not in ('en', 'ko'):
        raise ValueError('unknown language: %s' % lang)
    last = -1
    for c in cues:
        t, s = c['t'], c['text']
        if not isinstance(t, int) or t < 0:
            raise ValueError('bad time: %r' % t)
        if t < last:
            raise ValueError('the start times go back at [%d]' % t)
        if '\n' in s or '\r' in s or '[' in s or ']' in s:
            raise ValueError('a line may not hold a line break or brackets: %r' % s)
        last = t
    path = lang_file(lang)
    text, bom, eol = read_text(path)
    found = blocks(text, lang)
    if cut not in found:
        raise ValueError('%s has no %s block' % (os.path.basename(path), cut))
    a, b, old = found[cut]
    if lang == 'en':  # a macro, the last line of its last block ends it
        ends_macro = not old[-1].rstrip().endswith('\\')
        new = ['    "[%d]%s" \\' % (c['t'], escape(c['text'])) for c in cues]
        if ends_macro:
            new[-1] = new[-1][:-2]
    else:
        new = ['    "[%d]%s"' % (c['t'], escape(c['text'])) for c in cues]
    lines = text.split('\n')
    lines[a:b] = new
    write_text(path, '\n'.join(lines), bom, eol)
    return '%s: %s %d lines' % (os.path.relpath(path, args.src), cut, len(new))


def run(cmd, cwd=None):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    out = (p.stdout + p.stderr).strip()
    if p.returncode != 0:
        raise RuntimeError('%s failed:\n%s' % (' '.join(cmd), out[-3000:]))
    return out


def glyphs():
    font = args.font or next((f for f in FONTS if os.path.exists(f)), None)
    if not font:
        raise RuntimeError('no Korean font found, give one with --font')
    out = run([sys.executable, os.path.join(args.src, 'tools', 'glyphs', 'ko_glyphs.py'), os.path.join(args.src, 'lang'), font])
    return out + ' (font %s)' % os.path.basename(font)


def build():
    script = os.path.join(args.src, 'platform', 'osx', 'build.sh')
    if sys.platform != 'darwin' or not os.path.exists(script):
        raise RuntimeError('the build runs on macOS only, build the game as usual')
    log = []
    for opt in ([], ['-d']):
        out = run(['bash', script] + opt)
        log.append(next((l for l in reversed(out.splitlines()) if '빌드 완료' in l or 'BUILD SUCCEEDED' in l), 'built %s' % ('Debug' if opt else 'Release')))
    return '\n'.join(log)


class Handler(BaseHTTPRequestHandler):
    def send(self, code, body, ctype='application/json; charset=utf-8'):
        data = body if isinstance(body, bytes) else body.encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = self.path.split('?')[0]
        if path in ('/', '/index.html', '/subtitle_sync.html'):
            return self.send(200, open(PAGE, 'rb').read(), 'text/html; charset=utf-8')
        if path == '/api/subs':
            try:
                return self.send(200, json.dumps({'ko': subs('ko'), 'en': subs('en'), 'music': bool(args.music)}, ensure_ascii=False))
            except Exception as e:
                return self.send(500, json.dumps({'error': str(e)}))
        if path.startswith('/audio/') and args.music:
            name = os.path.basename(path[len('/audio/'):])
            file = os.path.join(args.music, name)
            if re.fullmatch(r'[\w.-]+', name) and os.path.isfile(file):
                return self.send(200, open(file, 'rb').read(), 'audio/ogg')
        self.send(404, json.dumps({'error': 'not found'}))

    def do_POST(self):
        if self.path != '/api/apply':
            return self.send(404, json.dumps({'error': 'not found'}))
        try:
            req = json.loads(self.rfile.read(int(self.headers.get('Content-Length', 0))))
            with lock:
                log = [apply(req['lang'], req['cut'], req['cues'])]
                if req['lang'] == 'ko':
                    log.append(glyphs())
                if req.get('build'):
                    log.append(build())
            self.send(200, json.dumps({'log': '\n'.join(log)}, ensure_ascii=False))
        except Exception as e:
            self.send(400, json.dumps({'error': str(e)}, ensure_ascii=False))

    def log_message(self, fmt, *a):
        pass


def main():
    global args
    p = argparse.ArgumentParser(description='Local server of the subtitle sync editor')
    p.add_argument('--src', default=os.path.normpath(os.path.join(HERE, '..', '..')), help='the src directory of OpenLara')
    p.add_argument('--music', help='the music folder of the game (Track02.ogg ...)')
    p.add_argument('--font', help='the Korean font for new glyphs (Malgun Gothic matches the old ones)')
    p.add_argument('--port', type=int, default=8765)
    p.add_argument('--no-browser', action='store_true')
    args = p.parse_args()
    if args.music:
        args.music = os.path.expanduser(args.music)
    url = 'http://127.0.0.1:%d/' % args.port
    server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    print('subtitle sync editor: %s (src %s)' % (url, args.src))
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
