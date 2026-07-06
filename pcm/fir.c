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

	if (p->fmt.fs != ir->fmt.fs) {
		debug("警告：標本化周波数が異なります（入力 %u Hz，IR %u Hz）\n",
			p->fmt.fs, ir->fmt.fs);
	}

	// 入力信号のコピー＆延長
	int	len0 = p->len;
	int	dn = nb - 1;		// 畳み込みによる延長量
	int	len = len0 + dn;	// 延長後の標本数

	Wav	*out = pcmInit(p->fmt.bit, p->fmt.ch, p->fmt.fs, len);
	if (out == NULL) fatal(1, "出力用WAV確保失敗\n");

	// 音割れ防止：係数の絶対値和で正規化
	double	sumAbs = 0.0;
	for (int m = 0; m < nb; m++) sumAbs += fabs(b[m]);
	double	norm = (sumAbs > 0.0) ? 1.0 / sumAbs : 1.0;
	debug("インパルス応答の絶対値和 = %f，正規化係数 = %f\n", sumAbs, norm);

	// 各チャネルに畳み込みを適用
	for (int c = 0; c < p->fmt.ch; c++) {
		double	*x = p->val[c];
		double	*y = out->val[c];

		// y[n] = Σ b[m] * x[n - m]
		for (int m = 0; m < nb; m++) {
			for (int n = 0; n < len0; n++) {
				y[n + m] += b[m] * x[n] * norm;
			}
		}
	}

	pcmWrite(stdout, out);

	pcmFin(ir);
	pcmFin(p);
	pcmFin(out);
	return (0);
}
