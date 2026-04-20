// WAVファイル生成のサンプルアプリ（正弦波）
// Ver.2023.12.06
// コンパイル：$ cc sin.c -std=c99 -lpcm -lm -o sin
// 実行方法：$ ./sin [周波数or音名 [量子化深度]]
// 実行例：
// 	$ ./sin | paplay	# 440 Hz，中央（オクターブ４）ラ音
// 	$ ./sin 880 | paplay	# 880 Hz，上（オクターブ５）ラ音
// 	$ ./sin A3 | paplay	# 220 Hz，下（オクターブ３）ラ音
// 周波数：数字列	# 単位：Hz	# 既定値："440"
// 音名：英字[半音記号][オクターブ番号]
// 	音名の例：中央ラ→ "A4"（既定値），
//		中央ド→ "C4"，上ド→ "C5"，下ド→ "C3"，
//		中央ミ♭ → "E-4"，中央ファ♯ → "F+4"，
// 	英字：ド→ "C"，レ→ "D"，...，ラ→ "A"（既定値），シ→ "B"
// 		# 大文字／小文字どちらでもOK
// 	半音記号：♭ → "-" or "b"，♯ → "+" or "#"
// 	オクターブ番号："0"，"1"，...，"4"（既定値），...，"9"
// 量子化深度："8"，"16"（既定値），"24"，"32"

#include <stdio.h>
#include <stdlib.h>
#define	_USE_MATH_DEFINES	// for M_PI on Windows
#define	__USE_XOPEN		// for M_PI on GCC/Linux
#include <math.h>
#include <string.h>
#include "pcm.h"

#define	debug(...)	fprintf(stderr, __VA_ARGS__)
#define	fatal(s, ...)	{ debug(__VA_ARGS__); exit(s); }

int main(int argc, char *argv[])
{
	double	f = pcmMusicFreq("A4");	// 信号音の周波数，"A4"=440[Hz]
	if (argc > 1) f = pcmMusicFreq(argv[1]);

	double	w = 2.0*M_PI*f;	// 信号音の角周波数ω

	int	bit = 16;	// 量子化深度，8|16|24|32
	if (argc > 2) bit = atoi(argv[2]);
	int	ch = 1;		// チャネル数，1|2
	int	fs = 48000;	// 標本化周波数，fs = 1/Δt
	double	d = 1.0;	// 音長 [sec]

	int	len = d*fs;	// 標本数，len = d/Δt = d*fs
	Wav	*p = pcmInit(bit, ch, fs, len);
	if (p == NULL) return (1);

	debug("sin 信号周波数 = %f\n", f);
	pcmInfo(stderr, p);

	// 実数による標本値の設定
	double	*x0 = p->val[0];	// 左チャネル標本値配列
//	double	*x1 = p->val[1];	// 右チャネル標本値配列，ch=1では不使用
	for (int i = 0; i < len; i++) {
		double	t = (double)i/(double)(fs);
		x0[i] = sin(w*t);
//		x1[i] = sin(w*t);
	}
		// 適切な値域：-1.0 <= x(t) < +1.0
		// 超えるとリミッタが発動し，波形が歪みますよ
	pcmWrite(stdout, p);
		// 標本の実数値は出力時に自動的にバイナリ化されます
	pcmFin(p);
	return (0);
}

