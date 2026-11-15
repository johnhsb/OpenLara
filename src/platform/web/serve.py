#!/usr/bin/env python3
"""Serves the web version of OpenLara on this computer with the data of the PC versions of the games.

    python3 serve.py --tr1 <Tomb Raider 1 folder> --tr2 <Tomb Raider 2 folder> [--port 8000]

A game folder is the one that holds data (DATA) and audio or music. The files are read from there as the
page asks for them (level/1/LEVEL1.PHD, audio/2/track_05.ogg...), nothing is copied or changed.
The page (index.html, OpenLara.js, OpenLara.wasm) is served from the folder of this script.
"""
import argparse
import http.server
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAMES = {}  # '1' or '2' -> the folder of the game


def find(folder, *names):
    """The path of names (a subfolder, then a file) under folder whatever their case, None if missing."""
    path = folder
    for name in names:
        try:
            entries = os.listdir(path)
        except OSError:
            return None
        match = [e for e in entries if e.lower() == name.lower()]
        if not match:
            return None
        path = os.path.join(path, match[0])
    return path


def game_file(kind, game, name):
    """The file of a game for level/<game>/<name> or audio/<game>/<name>."""
    folder = GAMES.get(game)
    if not folder or '/' in name or '\\' in name or name.startswith('.'):
        return None
    if kind == 'level' or name.lower() == 'main.sfx':
        return find(folder, 'data', name)
    track = re.match(r'track_(\d+)\.ogg$', name.lower())
    if track:  # the music pack of the PC version of TR2: music/TrackNN.ogg
        path = find(folder, 'music', 'Track%s.ogg' % track.group(1))
        if path:
            return path
    return find(folder, 'audio', name)


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = dict(http.server.SimpleHTTPRequestHandler.extensions_map, **{'.wasm': 'application/wasm'})

    def translate_path(self, path):
        match = re.match(r'/(level|audio)/([12])/([^/?#]+)', path)
        if match:
            found = game_file(*match.groups())
            if found and os.path.isfile(found):
                return found
        return super().translate_path(path)

    def log_message(self, fmt, *args):
        if len(args) > 1 and str(args[1]) == '404':  # the files the page looks for and a PC version has not
            return
        super().log_message(fmt, *args)

    def log_error(self, fmt, *args):
        if args and args[0] == 404:
            return
        super().log_error(fmt, *args)


def main():
    parser = argparse.ArgumentParser(description='Serves the web version of OpenLara on this computer.')
    parser.add_argument('--tr1', metavar='DIR', help='the folder of Tomb Raider 1 (with DATA and audio)')
    parser.add_argument('--tr2', metavar='DIR', help='the folder of Tomb Raider 2 (with data and audio or music)')
    parser.add_argument('--port', type=int, default=8000)
    args = parser.parse_args()

    for game, folder in (('1', args.tr1), ('2', args.tr2)):
        if not folder:
            continue
        if not find(folder, 'data'):
            sys.exit('no data folder in %s' % folder)
        GAMES[game] = os.path.abspath(folder)
    if not GAMES:
        parser.print_help()
        sys.exit(1)
    if not os.path.isfile(os.path.join(HERE, 'OpenLara.wasm')):
        sys.exit('OpenLara.wasm is not next to this script (build it with build.sh, or run the script of the release)')

    handler = lambda *a, **k: Handler(*a, directory=HERE, **k)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', args.port), handler)  # this computer only
    base = 'http://127.0.0.1:%d/' % args.port
    if '1' in GAMES:
        print('Tomb Raider 1:  %s' % base)
    if '2' in GAMES:
        print('Tomb Raider 2:  %s?level=level/2/TITLE.TR2' % base)
    print('Ctrl+C stops the server.')
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
