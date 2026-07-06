// インパルス信号の生成
// Ver.2026.07.06
// コンパイル：$ cc ip.c -std=c99 -I. -L. -lpcm -lm -o ip
// 実行例：$ ./ip > ip.wav
// 　　　　$ ./ip 2.0 > ip.wav   # 音長を 2.0 秒に
//
// 先頭 1 標本に振幅 0.9 のインパルスを与え，あとは無音．
// この信号を IIR リバーブフィルタに通すことでインパルス応答を取得する．

#include <stdio.h>
#include <stdlib.h>
#include "pcm.h"

#define	debug(...)	fprintf(stderr, __VA_ARGS__)
#define	fatal(s, ...)	{ debug(__VA_ARGS__); exit(s); }

int main(int argc, char *argv[])
{
	int	bit = 16;
	int	ch = 1;
	int	fs = 48000;
	double	d = 2.0;		// 既定音長 [s]
	if (argc > 1) d = atof(argv[1]);

	int	len = (int)(d * fs);
	if (len <= 0) fatal(1, "音長が不正です：%f\n", d);

	Wav	*p = pcmInit(bit, ch, fs, len);
	if (p == NULL) fatal(1, "WAV構造体の生成失敗\n");

	// 先頭 1 標本のみ振幅 0.9，残りは 0
	p->val[0][0] = 0.9;

	debug("■ インパルス生成\n");
	pcmInfo(stderr, p);

	pcmWrite(stdout, p);
	pcmFin(p);
	return (0);
}
