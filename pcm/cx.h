/*
 * cx.h - 複素数計算用補助ライブラリ
 * Version: 2023.12.13
 * 著者：yanagawa@kushiro-ct.ac.jp
 * 言語標準：C99
 */

#ifndef	CX_H
#define	CX_H

#ifdef __cplusplus
extern "C" {
#endif

// 円周率マクロ定数 M_PI を利用可能に
#define	_USE_MATH_DEFINES	// for M_PI on Windows
#define	__USE_XOPEN		// for M_PI on GCC/Linux
#include <math.h>

// よく利用する複素数計算を簡便に
#include <complex.h>
typedef	double complex	Cx;	// 複素数型：Cx
#define	J	1.0i		// 虚数単位jのマクロ定数：J
#define	J2Pi	2.0i*M_PI	// j 2πのマクロ定数：J2Pi
#define	Pi2	2.0*M_PI	// 2πのマクロ定数：Pi2
#define	ExpJ(rad)	cexp(J*(rad))	// exp(j a)の引数付きマクロ：ExpJ(a)
#define	Rot(N, kn)	cexp(-J2Pi*(kn)/(N))	// 回転子W_N^{kn}の引数付きマクロ：Rot(N, kn)

#ifdef __cplusplus
}
#endif

#endif
