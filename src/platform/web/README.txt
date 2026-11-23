OpenLara (web) - https://github.com/johnhsb/OpenLara

The game data is not included. You need the data of the PC version of Tomb Raider 1 and/or 2,
and Python 3.

    python3 serve.py --tr1 <Tomb Raider 1 folder> --tr2 <Tomb Raider 2 folder>

(on Windows: py serve.py ...). A game folder is the one that holds "data" and "audio" or "music".
Then open the address it prints (http://127.0.0.1:8000/) in a browser. One game is enough.
The files are read from the game folders as they are, nothing is copied. The server answers this
computer only. --port <number> picks another port.

Or start serve.py without a game folder and press "Open Game Folder" on the page: the browser keeps
the files of the game it needs (nothing is sent anywhere) and finds them on the next visit too.
"Forget Game Data" removes them. The same page is on https://johnhsb.github.io/OpenLara/

게임 데이터는 들어 있지 않습니다. Tomb Raider 1 / 2 PC판의 데이터와 Python 3이 필요합니다.
위 명령으로 서버를 띄우고, 출력된 주소를 브라우저로 엽니다. 게임 폴더는 data 와 audio(또는 music)가
들어 있는 폴더입니다. 한 게임만 지정해도 됩니다. 웹 버전에는 텍스처 팩이 적용되지 않습니다.
게임 폴더 없이 서버를 띄우고 페이지의 "Open Game Folder"로 폴더를 골라도 됩니다. 파일은 브라우저에만
보관되고 어디로도 보내지 않습니다. 같은 페이지가 https://johnhsb.github.io/OpenLara/ 에 있습니다.
