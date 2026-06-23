// 高速フーリエ変換（DFT → FFT の発展）
// Ver.2026.06.22
// コンパイル：$ cc ft.c -std=c99 -lpcm -lm -o ft
// 実行方法：$ ./ft [入力ファイル.wav [標本数 [始点]]]
//   ※ 標本数 N は 2 のべき乗に切り下げられます（radix-2 Cooley-Tukey の制約）

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

// === 素朴な DFT / IDFT（計算量 O(N^2)、比較用） ===

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

// === FFT / IFFT（radix-2 Cooley-Tukey, in-place, 計算量 O(N log N)） ===

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

// バタフライ演算（共通）
// sign = -1 で FFT, sign = +1 で IFFT
void Butterfly(Cx *a, int N, int sign)
{
	for (int len = 2; len <= N; len <<= 1) {
		double	theta = sign * Pi2 / (double)len;
		Cx	wlen = cos(theta) + sin(theta) * I;	// W_len^1
		for (int i = 0; i < N; i += len) {
			Cx	w = 1.0;
			for (int k = 0; k < len / 2; k++) {
				Cx	u = a[i + k];
				Cx	t = a[i + k + len/2] * w;
				a[i + k]          = u + t;
				a[i + k + len/2]  = u - t;
				w *= wlen;	// 回転子を漸次的に更新（cexp 呼び出しを削減）
			}
		}
	}
}

void FFT(Cx *x, int N)
{
	BitReverse(x, N);
	Butterfly(x, N, -1);
}

void IFFT(Cx *X, int N)
{
	BitReverse(X, N);
	Butterfly(X, N, +1);
	for (int n = 0; n < N; n++) X[n] /= (double)N;
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
	char	*wav = "-";	// 入力WAVファイル名
	int	N0 = 1024;	// 分析対象区間の標本数（指定値）
	int	n0 = 0;		// 分析対象区間の始点
	if (argc > 1) wav = argv[1];
	if (argc > 2) N0 = atoi(argv[2]);
	if (argc > 3) n0 = atoi(argv[3]);

	Wav	*p = pcmLoad(wav);
	if (p == NULL) return (1);
	debug("■ 入力データ\n");
	pcmInfo(stderr, p);

	// N を 2 のべき乗に切り下げ（FFT の制約）
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
		debug("エラー：分析区間がデータ長を超えています（n0=%d, N=%d, len=%u）\n", n0, N, p->len);
		pcmFin(p);
		return (1);
	}

	debug("■ 分析条件\n");
	debug("開始番号 = %d，\t開始時刻 = %f [s]\n", n0, (double)n0/p->fmt.fs);
	debug("標本数 = %d（2^%d），\t窓幅 = %f [s]\n",
		N, (__builtin_ctz(N)), (double)N/p->fmt.fs);
	debug("基本周波数 = %f [Hz]\n", p->fmt.fs/(double)N);
	debug("\n");

	Cx	xd[N], xf[N];		// DFT 用 / FFT 用波形バッファ
	Cx	Xd[N], Xf[N];		// DFT 用 / FFT 用スペクトル
	Cx	Xf_save[N];		// FFT スペクトルの退避（IFFT 後の比較用）
	Cx	xd_back[N], xf_back[N];	// IDFT / IFFT で戻した波形
	double	dt = 1.0/p->fmt.fs;	// Δt = 1/f_s
	double	df = (double)p->fmt.fs/(double)N;	// Δf = f_s/N
	double	*v = &(p->val[0][n0]);		// 分析対象の標本値列の先頭アドレス
	for (int n = 0; n < N; n++) {
		xd[n] = xf[n] = v[n];	// 分析対象を両方のバッファへコピー
	}
	pcmFin(p);
	debug("■ 入力波形\n");
	Plot("wf.txt", xd, N, dt, creal);
	debug("\n");

	// --- DFT + IDFT ---
	double	t0 = Timer();
	DFT(xd, Xd, N);
	double	tDFT = Timer() - t0;
	debug("■ DFT\n処理時間 = %e [s]\n", tDFT);
	Plot("dft.txt", Xd, N, df, cabs);
	debug("\n");

	t0 = Timer();
	IDFT(Xd, xd_back, N);
	double	tIDFT = Timer() - t0;
	debug("■ IDFT\n処理時間 = %e [s]\n", tIDFT);
	Plot("idft.txt", xd_back, N, dt, creal);
	debug("\n");

	// --- FFT + IFFT ---
	t0 = Timer();
	FFT(xf, N);			// in-place で xf にスペクトルが入る
	double	tFFT = Timer() - t0;
	debug("■ FFT\n処理時間 = %e [s]\n", tFFT);
	for (int k = 0; k < N; k++) Xf[k] = xf[k];	// スペクトル退避
	for (int k = 0; k < N; k++) Xf_save[k] = Xf[k];	// 比較用に保存
	Plot("fft.txt", Xf, N, df, cabs);
	debug("\n");

	t0 = Timer();
	IFFT(Xf, N);			// in-place で Xf に時間波形が戻る
	double	tIFFT = Timer() - t0;
	debug("■ IFFT\n処理時間 = %e [s]\n", tIFFT);
	for (int n = 0; n < N; n++) xf_back[n] = Xf[n];
	Plot("ifft.txt", xf_back, N, dt, creal);
	debug("\n");

	// --- 比較サマリ（stderr へ出力） ---
	debug("■ 性能比較（N = %d）\n", N);
	debug("DFT  : %e s\n", tDFT);
	debug("IDFT : %e s\n", tIDFT);
	debug("FFT  : %e s\n", tFFT);
	debug("IFFT : %e s\n", tIFFT);
	if (tFFT > 0.0)
		debug("高速化比 DFT/FFT = %.2f 倍\n", tDFT / tFFT);
	if (tIFFT > 0.0)
		debug("高速化比 IDFT/IFFT = %.2f 倍\n", tIDFT / tIFFT);
	debug("\n");

	// --- 結果の一致検証（DFT と FFT のスペクトル差） ---
	double	maxErr = 0.0;
	for (int k = 0; k < N; k++) {
		double	e = cabs(Xd[k] - Xf_save[k]);
		if (e > maxErr) maxErr = e;
	}
	debug("■ スペクトル一致検証\n");
	debug("max |Xd[k] - Xf[k]| = %e（理論上は丸め誤差程度）\n", maxErr);
	debug("\n");

	// --- 波形復元検証（入力波形と IFFT/IDFT 後の差） ---
	double	maxErrWaveFFT = 0.0, maxErrWaveDFT = 0.0;
	for (int n = 0; n < N; n++) {
		double	eF = cabs(xf_back[n] - v[n]);
		double	eD = cabs(xd_back[n] - v[n]);
		if (eF > maxErrWaveFFT) maxErrWaveFFT = eF;
		if (eD > maxErrWaveDFT) maxErrWaveDFT = eD;
	}
	debug("■ 波形復元検証\n");
	debug("max |IFFT(X) - x|    = %e\n", maxErrWaveFFT);
	debug("max |IDFT(Xd) - x|    = %e\n", maxErrWaveDFT);
	debug("\n");

	return (0);
}