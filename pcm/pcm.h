/*
 * libpcm.so - PCM用WAVファイル入出力ライブラリ
 * Version：2023.12.08
 * 著者：yanagawa@kushiro-ct.ac.jp
 *
 * 想定データの形式：非圧縮リニアPCM，8*nビット，1or2チャンネル
 * 想定WAVファイルの構成：｛RIFFヘッダ，...，fmtチャンク，dataチャンク，...｝
 * 	# 「...」部分は無視
 * 想定システムのバイトオーダ：リトルエンディアン
 * 	# RIFFファイル内の数値データはリトルエンディアンなので，
 * 	# Intel製CPU等のリトルエンディアンなシステムでは取り扱いが容易．
 * 	# ビッグエンディアンなシステムではバイトオーダ変換処理が必要．
 * ソースコードの言語標準：C99
 * ライブラリ関数の戻り値の流儀：0=NULL=エラー
 *
 * ライブラリのコンパイル方法（GCC）：$ cc -c pcm.c -std=c99 -fpic -shared -o libpcm.so
 * アプリのコンパイル方法（GCC）：$ cc ソース.c -lpcm -o プログラム
*/

#ifndef PCM_H
#define PCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

// PCM用データ形式構造体
typedef struct {
	unsigned short	type;	// 音声形式，1=非圧縮リニアPCM
	unsigned short	ch;	// チャネル数，1 or 2
	unsigned int	fs;	// 標本化周波数，大抵は 44,100 or 48,000 [Hz]
	unsigned int	dr;	// データレート（バイト数／秒）		[B/s]
	unsigned short	bs;	// ブロックサイズ（バイト数／標本）	[B]
	unsigned short	bit;	// 量子化深度，大抵は 8 or 16 [bit/channel]
} Fmt;		// 16バイト

// PCM用Wav構造体
typedef struct {
	// fmtチャンク部分
	unsigned int	fmtSize;	// fmtのデータサイズ, 16 [B]
	Fmt		fmt;		// データ形式

	// dataチャンク部分
	unsigned int	size;	// dataのデータサイズ	[B]
	unsigned char	*data;	// 標本のバイナリデータ列へのポインタ

	// 以上は手抜き実装のため，WAVファイル内の各チャンクと同形式としましたよ．
 	// したがって，改変すると異常動作のおそれあり．改変は危険．

	// 以下は独自拡張部分のため，WAVファイル内には記録されませんよ．
	// 改変は容易．
	unsigned int	len;	// 標本数
	double		time;	// 収録時間
	unsigned int	byte;	// バイト数／標本／チャネル
	double		*val[2];	// 標本の実数値配列へのポインタ
} Wav;

// PCM用Wav構造体の生成・初期化
extern Wav *pcmInit(unsigned int bit, unsigned int ch, unsigned int fs, unsigned int len);

// PCM用Wav構造体の破棄
extern void pcmFin(Wav *p);

// WAVファイルからPCMデータの生成
extern Wav *pcmRead(FILE *fp);
extern Wav *pcmLoad(const char *fn);

// PCMデータのWAVファイル出力
extern int pcmWrite(FILE *fp, const Wav *p);
extern int pcmSave(const char *fn, const Wav *p);

// PCMデータ属性のファイル出力
extern int pcmInfo(FILE *fp, const Wav *p);

// 音階名or周波数の文字列から周波数の数値への変換
extern double pcmMusicFreq(const char *s);
#define	F_A4	  440.0		// A4の周波数

#ifdef __cplusplus
}
#endif

#endif

