// WAVファイル生成アプリ（加算合成 + ADSR包絡線）
// 改造元ソース：sin.c
// コンパイル：$ cc synth.c -std=c99 -I. -L. -lpcm -lm -o synth
// 実行例：$ ./synth [音名or周波数] | paplay
// 　　　　$ ./synth C > C.wav

#include <stdio.h>
#include <stdlib.h>
#define	_USE_MATH_DEFINES
#define	__USE_XOPEN
#include <math.h>
#include <string.h>
#include "pcm.h"

#define	debug(...)	fprintf(stderr, __VA_ARGS__)
#define	fatal(s, ...)	{ debug(__VA_ARGS__); exit(s); }

// ADSR包絡線 e(t)
// A: アタックタイム [sec]
// D: ディケイタイム [sec]
// S: サステインレベル [0.0, 1.0]
// R: リリースタイム [sec]
// L: デュレーション [sec]
static double envelope(double t, double A, double D, double S, double R, double L)
{
	double G = L - R;	// ゲートタイム（持続終了時刻）
	if (t < A)
		return (A > 0) ? (1.0 - exp(-5.0 * t / A)) : 1.0;
	if (t < G)
		return (D > 0) ? (S + (1.0 - S) * exp(-5.0 * (t - A) / D)) : S;
	return (R > 0) ? (S * exp(-5.0 * (t - G) / R)) : 0.0;
}

int main(int argc, char *argv[])
{
	double	f = pcmMusicFreq("A4");	// 既定値：440 Hz
	if (argc > 1) f = pcmMusicFreq(argv[1]);

	int	bit = 16, ch = 1, fs = 48000;
	double	L = 1.0;		// デュレーション [sec]
	int	len = (int)(L * fs);	// 総標本数

	// ADSR パラメータ
	double	A = 0.01;	// アタックタイム  [sec]
	double	D = 0.15;	// ディケイタイム  [sec]
	double	S = 0.5;	// サステインレベル [0.0, 1.0]
	double	R = 0.3;	// リリースタイム  [sec]

	// スペクトル分布（矩形波：奇数次高調波のみ，a_k = 4/(π*k)）
	// 鋸歯状波にする場合：a[k] = 2.0 / (M_PI * k) のループを k+=1 で
	// 三角波にする場合：奇数次のみ a[k] = 8.0 / (M_PI*M_PI * k*k)
#define N 50
	double	a[N+1];
	for (int k = 0; k <= N; k++) a[k] = 0.0;
	for (int k = 1; k <= N; k += 2)
		a[k] = 4.0 / (M_PI * k);

	// 振幅スペクトルの正規化（合計が 1.0 になるように）
	double	a0 = 0.0;
	for (int k = 1; k <= N; k++) a0 += a[k];
	for (int k = 1; k <= N; k++) a[k] /= a0;

	Wav	*p = pcmInit(bit, ch, fs, len);
	if (p == NULL) return (1);

	debug("synth: f = %f Hz\n", f);
	pcmInfo(stderr, p);

	double	*x0 = p->val[0];
	for (int i = 0; i < len; i++) {
		double	t  = (double)i / fs;
		double	wt = 2.0 * M_PI * f * t;

		// 加算合成
		double	v = 0.0;
		for (int k = 1; k <= N; k++)
			v += a[k] * sin(k * wt);

		// ADSR 包絡線の適用
		x0[i] = v * envelope(t, A, D, S, R, L);
	}

	pcmWrite(stdout, p);
	pcmFin(p);
	return (0);
}
