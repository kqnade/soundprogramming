// WAVファイル入出力のサンプルアプリ（素通しフィルタ）
// コンパイル：$ cc thru.c -std=c99 -lpcm -o thru
// 実行例：$ ./thru < ファイル.wav | paplay
// 比較実行例：$ paplay ファイル.wav
// 	# 同じ音に聴こえたら成功．

#include <stdio.h>
#include <stdlib.h>
#include "pcm.h"

// デバッグ表示マクロ：printf() の標準エラー出力版
#define	debug(...)	fprintf(stderr, __VA_ARGS__)

// エラー表示・強制終了マクロ
#define	fatal(s, ...)	{ debug(__VA_ARGS__); exit(s); }

int main(int argc, char *argv[])
{
	Wav	*p = pcmRead(stdin);	// WAVファイルを標準入力，メモリ確保
//	Wav	*p = pcmLoad("-");		// ファイル名の指定で入力の場合
	if (p == NULL) return (1);
//	if (p == NULL) fatal(1, "Read失敗．入力が線形PCMじゃなさげ．\n");	// エラー表示必要な場合
//	debug("Readできた．\n");			// デバッグ表示必要な場合

	pcmInfo(stderr, p);		// PCMデータ属性を表示

	pcmWrite(stdout, p);		// WAVファイルを標準出力
//	pcmSave("-", p);			// ファイル名の指定で出力の場合
//	debug("Writeもできた．\n");

	pcmFin(p);			// メモリ解放
	return (0);
}

