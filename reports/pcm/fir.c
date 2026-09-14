// FIR フィルタ（インパルス応答による畳み込み）
// Ver.2026.07.06
// コンパイル：$ cc fir.c -std=c99 -I. -L. -lpcm -lm -o fir
// 実行例：$ cat input.wav | ./fir ir.wav | paplay
// 　　　　$ cat input.wav | ./fir ir.wav > output.wav
//
// 第 1 引数にインパルス応答ファイル（既定値 "ir.wav"）を指定する．
// 入力信号とインパルス応答を畳み込み，IIR フィルタの特性を再現する．

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include "pcm.h"

#define	debug(...)	fprintf(stderr, __VA_ARGS__)
#define	fatal(s, ...)	{ debug(__VA_ARGS__); exit(s); }

int main(int argc, char *argv[])
{
	// フィルタ係数（インパルス応答）の入力
	char	*fn = "ir.wav";
	if (argc > 1) fn = argv[1];
	Wav	*ir = pcmLoad(fn);
	if (ir == NULL) fatal(1, "ir: Read失敗\n");

	debug("■ FIR フィルタ\n");
	debug("インパルス応答ファイル = %s\n", fn);
	pcmInfo(stderr, ir);

	double	*b = ir->val[0];
	int	nb = ir->len;

	// 入力信号の読込
	Wav	*p = pcmRead(stdin);
	if (p == NULL) fatal(1, "入力WAVの読込失敗\n");

	if (p->fmt.fs != ir->fmt.fs || ir->fmt.ch != 1) {
		pcmFin(ir);
		pcmFin(p);
		fatal(1, "IRは入力と同じ標本化周波数のモノラルPCMが必要です\n");
	}

	// 入力信号のコピー＆延長
	if (p->len > (unsigned int)INT_MAX - ir->len + 1) {
		pcmFin(ir); pcmFin(p);
		fatal(1, "出力が長すぎます\n");
	}
	int	len0 = p->len;
	int	dn = nb - 1;		// 畳み込みによる延長量
	int	len = len0 + dn;	// 延長後の標本数

	Wav	*out = pcmInit(p->fmt.bit, p->fmt.ch, p->fmt.fs, len);
	if (out == NULL) fatal(1, "出力用WAV確保失敗\n");

	// 係数の利得を保つ。入力振幅はインパルス応答の絶対値和を考慮して設定する。
	double sumAbs = 0.0;
	for (int m = 0; m < nb; m++) sumAbs += fabs(b[m]);
	debug("インパルス応答の絶対値和 = %f\n", sumAbs);

	// 各チャネルに畳み込みを適用
	for (int c = 0; c < p->fmt.ch; c++) {
		double	*x = p->val[c];
		double	*y = out->val[c];

		// y[n] = Σ b[m] * x[n - m]
		for (int m = 0; m < nb; m++) {
			if (b[m] == 0.0) continue;
			for (int n = 0; n < len0; n++) {
				y[n + m] += b[m] * x[n];
			}
		}
	}

	pcmWrite(stdout, out);

	pcmFin(ir);
	pcmFin(p);
	pcmFin(out);
	return (0);
}
