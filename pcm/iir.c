// IIR 簡易リバーブフィルタ
// Ver.2026.07.06
// コンパイル：$ cc iir.c -std=c99 -I. -L. -lpcm -lm -o iir
// 実行例：$ cat input.wav | ./iir | paplay
// 　　　　$ cat input.wav | ./iir > output.wav
//
// フィルタ構成（並列帰還型）：
//   y[n] = b0 * x[n] + Σ a[m] * y[n - d[m]]
//
// パラメータは下記の配列で変更可能．
// 発散・発振を防ぐため，係数の絶対値は 1.0 未満に保つこと．

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcm.h"

#define	debug(...)      fprintf(stderr, __VA_ARGS__)
#define	fatal(s, ...)   { debug(__VA_ARGS__); exit(s); }

// === フィルタパラメータ ===
// 入力ゲイン：音割れ防止のため 0.5 程度に抑える
static const double	b0 = 0.5;

// 帰還遅延と帰還係数（並列経路）
// 距離 10 [m] 程度の反響を想定すると，fs=48kHz で約 3000 標本となる
// 発散防止のため，帰還係数の絶対値の総和は 1.0 未満にする
static const int	delay[] = { 1009, 2011, 3001 };
static const double	alpha[] = { 0.40, 0.30, 0.20 };
static const int	N_DELAY = sizeof(delay) / sizeof(delay[0]);

int main(int argc, char *argv[])
{
	Wav	*p = pcmRead(stdin);
	if (p == NULL) fatal(1, "入力WAVの読込失敗\n");

	debug("■ IIR 簡易リバーブフィルタ\n");
	pcmInfo(stderr, p);

	// 最大遅延量の計算
	int	dmax = 0;
	for (int i = 0; i < N_DELAY; i++) {
		if (delay[i] > dmax) dmax = delay[i];
		debug("帰還経路 %d：遅延 = %d 標本，係数 = %f\n",
			i + 1, delay[i], alpha[i]);
	}
	debug("入力ゲイン b0 = %f\n", b0);
	debug("最大遅延量 = %d 標本（約 %.3f [s]）\n",
		dmax, (double)dmax / p->fmt.fs);
	debug("\n");

	// 各チャネルに同じフィルタを適用
	for (int c = 0; c < p->fmt.ch; c++) {
		unsigned int	len = p->len;
		double		*x = p->val[c];

		// 出力バッファ：入出力＋遅延部を確保してゼロ初期化
		// y[n + dmax] が n 番目の出力に対応
		double	*y = (double *)calloc(len + dmax, sizeof(double));
		if (y == NULL) fatal(1, "出力バッファ確保失敗\n");

		for (unsigned int n = 0; n < len; n++) {
			double	s = b0 * x[n];
			for (int i = 0; i < N_DELAY; i++) {
				s += alpha[i] * y[n + dmax - delay[i]];
			}
			y[n + dmax] = s;
		}

		// 結果を元の配列に書き戻す
		for (unsigned int n = 0; n < len; n++) {
			x[n] = y[n + dmax];
		}

		free(y);
	}

	pcmWrite(stdout, p);
	pcmFin(p);
	return (0);
}
