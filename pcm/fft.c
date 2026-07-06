// 高速フーリエ変換による周波数特性の算出
// Ver.2026.07.06
// コンパイル：$ cc fft.c -std=c99 -I. -L. -lpcm -lm -o fft
// 実行方法：$ ./fft [入力ファイル.wav [標本数 [始点]]]
//   ※ 標本数 N は 2 のべき乗に切り下げられます
//
// dft.c から，処理時間測定・DFT/IDFT 呼出を削除し，FFT() のみを呼び出す版．
// 入力波形を wf.txt，振幅スペクトルを fft.txt に出力する．

#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include "pcm.h"
#include "cx.h"

#define	debug(...)	fprintf(stderr, __VA_ARGS__)
#define	fatal(s, ...)	{ debug(__VA_ARGS__); exit(s); }

// グラフ用データ出力関数
void Plot(const char *file, const Cx *v, int N, double d, double (*func)(Cx v))
{
	FILE	*fp = fopen(file, "w");
	if (!fp) fatal(1, "%s オープン失敗\n", file);

	for (int i = 0; i < N; i++) {
		fprintf(fp, "%e\t%e\n", i*d, func(v[i]));
	}
	debug("出力ファイル = %s\n", file);
	fclose(fp);
}

// ビット逆順ソート（in-place）
void BitReverse(Cx *a, int N)
{
	for (int i = 1, j = 0; i < N; i++) {
		int bit = N >> 1;
		for (; j & bit; bit >>= 1) {
			j ^= bit;
		}
		j ^= bit;
		if (i < j) {
			Cx t = a[i];
			a[i] = a[j];
			a[j] = t;
		}
	}
}

// バタフライ演算（sign = -1 で FFT, +1 で IFFT）
void Butterfly(Cx *a, int N, int sign)
{
	for (int len = 2; len <= N; len <<= 1) {
		double	theta = sign * Pi2 / (double)len;
		Cx	wlen = cos(theta) + sin(theta) * I;
		for (int i = 0; i < N; i += len) {
			Cx w = 1.0;
			for (int k = 0; k < len / 2; k++) {
				Cx u = a[i + k];
				Cx t = a[i + k + len/2] * w;
				a[i + k]         = u + t;
				a[i + k + len/2] = u - t;
				w *= wlen;
			}
		}
	}
}

void FFT(Cx *x, int N)
{
	BitReverse(x, N);
	Butterfly(x, N, -1);
}

// N を 2 のべき乗に切り下げ
int FloorPow2(int N)
{
	int p = 1;
	while ((p << 1) <= N) p <<= 1;
	return p;
}

int main(int argc, char *argv[])
{
	char	*wav = "-";
	int	N0 = 8192;
	int	n0 = 0;
	if (argc > 1) wav = argv[1];
	if (argc > 2) N0 = atoi(argv[2]);
	if (argc > 3) n0 = atoi(argv[3]);

	Wav	*p = pcmLoad(wav);
	if (p == NULL) return (1);
	debug("■ 入力データ\n");
	pcmInfo(stderr, p);

	int	N = FloorPow2(N0);
	if (N != N0)
		debug("注：N=%d は 2 のべき乗でないため %d に切り下げます\n", N0, N);

	if (N <= 0) {
		debug("エラー：標本数Nは正の整数が必要です（N=%d）\n", N);
		pcmFin(p);
		return (1);
	}
	if (n0 < 0) {
		debug("エラー：始点n0は0以上が必要です（n0=%d）\n", n0);
		pcmFin(p);
		return (1);
	}
	if (p->fmt.fs == 0) {
		debug("エラー：標本化周波数fsが不正です（fs=%u）\n", p->fmt.fs);
		pcmFin(p);
		return (1);
	}
	if ((unsigned long long)n0 + (unsigned long long)N > (unsigned long long)p->len) {
		debug("エラー：分析区間がデータ長を超えています（n0=%d, N=%d, len=%u）\n",
			n0, N, p->len);
		pcmFin(p);
		return (1);
	}

	debug("■ 分析条件\n");
	debug("開始番号 = %d，\t開始時刻 = %f [s]\n", n0, (double)n0/p->fmt.fs);
	debug("標本数 = %d（2^%d），\t窓幅 = %f [s]\n",
		N, (__builtin_ctz(N)), (double)N/p->fmt.fs);
	debug("基本周波数 = %f [Hz]\n", p->fmt.fs/(double)N);
	debug("\n");

	Cx	x[N];
	double	dt = 1.0/p->fmt.fs;
	double	df = (double)p->fmt.fs/(double)N;
	double	*v = &(p->val[0][n0]);
	for (int n = 0; n < N; n++) x[n] = v[n];
	pcmFin(p);

	debug("■ 入力波形\n");
	Plot("wf.txt", x, N, dt, creal);
	debug("\n");

	FFT(x, N);
	debug("■ FFT 振幅スペクトル（伝達関数の推定）\n");
	Plot("fft.txt", x, N, df, cabs);
	debug("\n");

	return (0);
}
