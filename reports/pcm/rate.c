// WAVファイル入出力のサンプルアプリ（倍速再生フィルタ）
// コンパイル：$ cc rate.c -std=c99 -lpcm -o rate
// 実行例：$ ./rate 2.0 < ファイル.wav | paplay
// 	# ２倍速で再生されたら成功．

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include "pcm.h"

#define	debug(...)	fprintf(stderr, __VA_ARGS__)
#define	fatal(s, ...)	{ debug(__VA_ARGS__); exit(s); }

int main(int argc, char *argv[])
{
	double	rate = 2.0;	// 標本化周波数の倍率
	if (argc > 1) {
		char *end;
		rate = strtod(argv[1], &end);
		if (end == argv[1] || *end || !isfinite(rate) || rate <= 0)
			fatal(1, "倍率が不正です\n");
	}
	debug("倍率 = %f\n", rate);

	Wav	*p = pcmRead(stdin);
	if (p == NULL) return (1);

	debug("■ 元のPCM属性：\n");
	pcmInfo(stderr, p);

	double fs = p->fmt.fs * rate;
	if (fs < 1 || fs > UINT_MAX / p->fmt.bs) {
		pcmFin(p);
		fatal(1, "標本化周波数が範囲外です\n");
	}
	p->fmt.fs = (unsigned int)fs;	// 整数 Hz に切り捨て
	p->fmt.dr = p->fmt.fs * p->fmt.bs;
	p->time = (double)p->len / p->fmt.fs;
	debug("■ 変更後のPCM属性：\n");
	pcmInfo(stderr, p);

	pcmWrite(stdout, p);
	pcmFin(p);
	return (0);
}

