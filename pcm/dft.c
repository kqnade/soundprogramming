// 離散フーリエ変換
// Ver.2023.12.15
// コンパイル：$ cc dft.c -std=c99 -lpcm -lm -o dft
// 実行方法：$ ./dft [入力ファイル.wav [標本数 [始点]]]

#include <stdio.h>
#include <stdlib.h>
#define	__USE_POSIX199309	// clock_gettime()
#include <time.h>
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

// 時間測定関数
double Timer()
{
	struct timespec	ts;		// 時刻データの構造体

	clock_gettime(CLOCK_REALTIME, &ts);	// 現在時刻を取得
			// ts.tv_sec：	時刻の整数成分（s；秒）
			// ts.tv_nsec：	時刻の小数成分（ns；ナノ秒）

	return (ts.tv_sec + ts.tv_nsec*1.0e-9);	// 1 ns = 1.0x10^-9 s
}

void DFT(const Cx *x, Cx *X, int N)
{
	for (int k = 0; k < N; k++) {
		Cx s = 0.0;
		for (int n = 0; n < N; n++) {
			s += x[n] * cexp(-J2Pi * (double)(k*n) / (double)N);
		}
		X[k] = s;
	}
}

void IDFT(const Cx *X, Cx *x, int N)
{
	for (int n = 0; n < N; n++) {
		Cx s = 0.0;
		for (int k = 0; k < N; k++) {
			s += X[k] * cexp(J2Pi * (double)(k*n) / (double)N);
		}
		x[n] = s / (double)N;
	}
}

int main(int argc, char *argv[])
{
	char	*wav = "-";	// 入力WAVファイル名
	int	N = 480;	// 分析対象区間の標本数
	int	n0 = 0;		// 分析対象区間の始点
	if (argc > 1) wav = argv[1];
	if (argc > 2) N = atoi(argv[2]);
	if (argc > 3) n0 = atoi(argv[3]);

	Wav	*p = pcmLoad(wav);
	if (p == NULL) return (1);
	debug("■ 入力データ\n");
	pcmInfo(stderr, p);

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
		debug("エラー：分析区間がデータ長を超えています（n0=%d, N=%d, len=%u）\n", n0, N, p->len);
		pcmFin(p);
		return (1);
	}

	debug("■ 分析条件\n");
	debug("開始番号 = %d，\t開始時刻 = %f [s]\n", n0, (double)n0/p->fmt.fs);
	debug("標本数 = %d，\t窓幅 = %f [s]\n", N, (double)N/p->fmt.fs);
	debug("基本周波数 = %f [Hz]\n", p->fmt.fs/(double)N);
	debug("\n");

	Cx	x[N], X[N];		// 波形とスペクトルのバッファ
	double	dt = 1.0/p->fmt.fs;	// Δt = 1/f_s
	double	df = (double)p->fmt.fs/(double)N;	// Δf = f_s/N
	double	*v = &(p->val[0][n0]);		// 分析対象の標本値列の先頭アドレス
	for (int n = 0; n < N; n++) x[n] = v[n];	// 分析対象をバッファへコピー
	pcmFin(p);
	debug("■ 入力波形\n");
	Plot("wf.txt", x, N, dt, creal);
	debug("\n");

	double	t0 = Timer();
	DFT(x, X, N);
	debug("■ DFT\n処理時間 = %f [s]\n", Timer() - t0);
	Plot("dft.txt", X, N, df, cabs);
	debug("\n");

	t0 = Timer();
	IDFT(X, x, N);
	debug("■ IDFT\n処理時間 = %f [s]\n", Timer() - t0);
	Plot("idft.txt", x, N, dt, creal);
	debug("\n");
	return (0);
}
