// WAVファイル入出力のサンプルアプリ（倍速再生フィルタ）
// コンパイル：$ cc rate.c -std=c99 -lpcm -o rate
// 実行例：$ ./rate 2.0 < ファイル.wav | paplay
// 	# ２倍速で再生されたら成功．

#include <stdio.h>
#include <stdlib.h>
#include "pcm.h"

#define	debug(...)	fprintf(stderr, __VA_ARGS__)
#define	fatal(s, ...)	{ debug(__VA_ARGS__); exit(s); }

int main(int argc, char *argv[])
{
	double	rate = 2.0;	// 標本化周波数の倍率
	if (argc > 1) rate = atof(argv[1]);
	debug("倍率 = %f\n", rate);

	Wav	*p = pcmRead(stdin);
	if (p == NULL) return (1);

	debug("■ 元のPCM属性：\n");
	pcmInfo(stderr, p);

	p->fmt.fs *= rate;	// 標本化周波数を変更
	debug("■ 変更後のPCM属性：\n");
	pcmInfo(stderr, p);

	pcmWrite(stdout, p);
	pcmFin(p);
	return (0);
}

