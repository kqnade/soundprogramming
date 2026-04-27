// WAVファイル生成のサンプルアプリ（鋸歯状波などの基本的波形）
// Ver.2023.12.07
// 改造元ソース：sin.c
// 言語標準：C99
// コンパイル：$ cc wave.c -std=c99 -lpcm -lm -o wave
// 実行例：$ ./wave [波形略称 [周波数or音名]] | paplay
// 波形略称："saw" | "tri" | "sqr" | "sin"

#include <stdio.h>
#include <stdlib.h>
#define	_USE_MATH_DEFINES	// for M_PI on Windows
#define	__USE_XOPEN		// for M_PI on GCC/Linux
#include <math.h>
#include <string.h>
#include "pcm.h"

#define	debug(...)	fprintf(stderr, __VA_ARGS__)
#define	fatal(s, ...)	{ debug(__VA_ARGS__); exit(s); }

// 基本的波形の瞬時値算出関数 ◯◯Wave(ft)
// ft = f*t = t/T	f：周波数，T：周期，t：時間

// 鋸歯状波	値域 [-1.0, +1.0)
double sawtoothWave(double ft)
{
	return (2.0*(ft - floor(ft)) - 1.0);
		// floor(x)：実数x の下方向への整数化
		// x - floor(x)：実数x の小数部，[0.0, 1.0)
}

// 三角波	値域 [-1.0, +1.0)
double triangleWave(double ft)
{
	double r = 4.0*(ft - floor(ft));	// 値域 [0, 4) の鋸歯状波
	if (r < 1.0) return (r);	// 区間 [0, 1/4) → 上り坂
	if (r < 3.0) return (2.0 - r);	// 区間 [1/4, 3/4) → 下り坂
	return (r - 4.0);		// 区間 [3/4, 4/4) → 上り坂
}

// 矩形波	値域 [-1.0, +1.0]
double squareWave(double ft)
{
	return ((sin(2*M_PI*ft) > 0) ? 1.0 : -1.0);	// sgn(sin(ωt))
}

// 正弦波	値域 [-1.0, +1.0]
double sineWave(double ft)
{
	return (sin(2*M_PI*ft));	// sin(ωt)
}

// 波形構造体
typedef struct {
	char	*tag;			// 略称
	char	*name;			// 名前
	double	(*val)(double ft);	// 瞬時値算出関数へのポインタ
} Waveform;
	// 関数ポインタは関数を間接的に呼び出すための変数．関数名が登録される．
	// 関数名が代入され，間接的に関数を呼び出せる．
	// 宣言・代入例：double (*func)(double x) = cos;
	// 利用例：a = func(0.0);	// → a = cos(0.0);

// 波形テーブル
Waveform	wfTable[] = {
	{"saw", "鋸歯状波", sawtoothWave},
	{"tri", "三角波", triangleWave},
	{"sqr", "矩形波", squareWave},
	{"sin", "正弦波", sineWave},
	{NULL, NULL, NULL}
};

// 波形検索関数
Waveform *wfQuery(char *tag)
{
	Waveform	*p = wfTable;
	while (p->tag) {
		if (strcmp(tag, p->tag) == 0) return (p);
		p++;
	}
	return (NULL);
}

int main(int argc, char *argv[])
{
	Waveform	*wf = wfQuery("saw");	// 波形の既定値
	if (argc > 1) {
		wf = wfQuery(argv[1]);
		if (!wf) fatal(1, "知らん波形でんがな：%s\n", argv[1]);
	}
	debug("%s\n", wf->name);

	double	f = pcmMusicFreq("A4");	// 信号音の周波数の既定値，"A4" = 440.0 Hz
	if (argc > 2) f = pcmMusicFreq(argv[2]);

//	double	w = 2.0*M_PI*f;	// 信号音の角周波数ω
	debug("信号音の基本周波数 f = %f\n", f);

	int	fs = 48000;	// 標本化周波数，fs = 1/Δt
	int	bit = 16;	// 量子化深度
	int	ch = 1;		// チャネル数
	double	d = 1.0;	// 音長 [sec]

	int	len = fs*d;	// 標本数，len = d/Δt = fs*d
	Wav	*p = pcmInit(bit, ch, fs, len);
	if (p == NULL) return (1);
//	pcmInfo(stderr, p);

	double	*v0 = p->val[0];	// 左チャネル標本値配列
	for (int i = 0; i < len; i++) {
		double	t = (double)i/(double)(fs);
//		double	wt = w*t;
//		v0[i] = sin(w*t);
		v0[i] = wf->val(f*t);
	}
	pcmWrite(stdout, p);
	pcmFin(p);
	return (0);
}

