ライブラリとサンプルアプリのコンパイル
$ make

WAVファイルの在処の調査
$ ls -R /usr/share/sounds/

サンプルアプリの実行
$ ./sin | paplay

$ for note in C D E F G A B C5
> do
> ./sin $note | paplay
> done

$ ./thru < ファイル.wav | paplay

$ ./rate 2.0 < ファイル.wav | paplay

