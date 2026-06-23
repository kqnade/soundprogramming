# 高速フーリエ変換（FFT）の実装と DFT との比較レポート

> 課題 `sp-0106`（DFT から FFT を実装して報告せよ）
> 提出日時: 2026-06-22 17:00
> 対象ファイル: `pcm/ft.c`, `pcm/plot_ft.py`

---

## 1. 作成した `ft.c` のソースコード

`ft.c` は、前回実装した `dft.c` を拡張し、**素朴な DFT/IDFT と FFT/IFFT を同一プログラム内で実行し、両者の処理時間と結果を比較** できるようにしたプログラムである。

```c
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
				w *= wlen;	// 回転子を漸进的に更新（cexp 呼び出しを削減）
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
		debug("エラー：分析区間がデータ長を超えています（n0=%d, N=%d, len=%u)\n", n0, N, p->len);
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
```

**アルゴリズム (`radix-2 Cooley-Tukey`):**
本実装は以下の 3 ステップで構成される。

1. **`BitReverse`**: 配列 `a[n]` を、インデックス `n` のビット逆順へ並び替える。
2. **`Butterfly(a, N, sign)`**: `len` を 2, 4, 8, …, `N` と 2 倍ずつ進めながら、それぞれのサイズで蝶の羽のような演算（バタフライ）を施す。
   - 1 つの `w` から `wlen` を掛けて更新 = 回転子 $W_N^{kn}$ を再計算せず漸次的に求める。
   - `cexp()` は `len` ごとに 1 回しか呼ばれない（$O(N)$ 回）。これが FFT が $O(N \log N)$ になる本質。
3. **IFFT**: `sign = +1` で `Butterfly` を呼び出し、最後に $1/N$ でスケーリング。

### Makefile

```make
ft:	ft.c	$(LIB)
	cc $< -std=c99 -I. -L. -lpcm -lm -o $@
```

`make ft` でビルドできる。実行は `./ft A.wav 1024 0`。

---

## 2. 作成したプロットのスクリーンショット・結果（証拠）と 工夫点および性能向上への効果

### 2.1 実行結果（証拠）

`A.wav`（1 秒、48 kHz、モノラル、A4 = 440 Hz 相当の正弦波）を入力とし、
`N = 1024` で `ft` を実行したときの `stderr` 出力:

```
■ 入力データ
チャネル数：	1
標本化周波数：	48000 [Hz]
量子化深度：	16 [bits]
データサイズ：	96000 [bytes]
標本数：	48000
収録時間：	1.000000 [s]

■ 分析条件
開始番号 = 0，	開始時刻 = 0.000000 [s]
標本数 = 1024（2^10），	窓幅 = 0.021333 [s]
基本周波数 = 46.875000 [Hz]

■ DFT
処理時間 = 7.513309e-02 [s]
■ IDFT
処理時間 = 6.271720e-02 [s]
■ FFT
処理時間 = 8.893013e-05 [s]
■ IFFT
処理時間 = 9.703636e-05 [s]

■ 性能比較（N = 1024）
DFT  : 7.513309e-02 s
IDFT : 6.271720e-02 s
FFT  : 8.893013e-05 s
IFFT : 9.703636e-05 s
高速化比 DFT/FFT = 844.86 倍
高速化比 IDFT/IFFT = 646.33 倍

■ スペクトル一致検証
max |Xd[k] - Xf[k]| = 1.993786e-11（理論上は丸め誤差程度）

■ 波形復元検証
max |IFFT(X) - x|    = 1.194380e-14
max |IDFT(Xd) - x|    = 1.465695e-13
```

### 2.2 各種 `N` における処理時間（Apple MacBook Pro, x86_64）

| N     | DFT [s]      | IDFT [s]     | FFT [s]      | IFFT [s]     | 高速化比 DFT/FFT | 高速化比 IDFT/IFFT |
|------:|-------------:|-------------:|-------------:|-------------:|----------------:|-------------------:|
| 64    | 2.03 × 10⁻⁴ | 4.01 × 10⁻⁴ | 5.96 × 10⁻⁶ | 6.91 × 10⁻⁶ |          34.08 |              58.00 |
| 256   | 3.15 × 10⁻³ | 3.40 × 10⁻³ | 2.19 × 10⁻⁵ | 2.31 × 10⁻⁵ |         143.79 |             146.93 |
| 1024  | 7.51 × 10⁻² | 6.27 × 10⁻² | 8.89 × 10⁻⁵ | 9.70 × 10⁻⁵ |         844.86 |             646.33 |
| 4096  | 1.14 × 10⁰  | 1.35 × 10⁰  | 4.52 × 10⁻⁴ | 4.62 × 10⁻⁴ |        2517.34 |            2920.23 |
| 8192  | 4.79 × 10⁰  | 4.82 × 10⁰  | 9.38 × 10⁻⁴ | 1.22 × 10⁻³ |        5108.43 |            3947.97 |

`/Users/kqnade/repos/github.com/kqnade/soundprogramming/pcm/ft_timing.png` にロググラフ化してある。
DFT 側は `N²` の直線、FFT 側は `N log N` の直線に載っており、N が大きくなるほど開きが広がる。

### 2.3 スクリーンショット（証拠画像）

以下の 3 枚を `pcm/` に保存済み:

1. **`ft_waveform.png`** :
   上段 = 入力波形 `x[n]`（`A.wav` 先頭 1024 サンプル = 21.3 ms）、下段 = IDFT と IFFT で戻した波形の重ね書き。両者は完全に一致し、視覚的に区別できない（IFFT は橙色の破線）。
2. **`ft_spectrum.png`** :
   上段 = DFT による `|X_d[k]|`、下段 = FFT による `|X_f[k]|`。ピーク位置もピーク高さも両者で同一であり、FFT の結果が DFT と数学的に等価であることが目視でも確認できる。
3. **`ft_timing.png`** :
   各 `N` に対する DFT/IDFT と FFT/IFFT の処理時間を両対数グラフに描画。点線は $O(N^2)$ と $O(N \log N)$ の理論カーブで、実測値がよく乗っている。

### 2.4 工夫点と性能向上への効果

#### 2.4.1 `radix-2 Cooley-Tukey` による $O(N \log N)$ 化
- **工夫**: 標本数 $N$ を段階的に半分 ずつ分解（divide-and-conquer）。偶数番目と奇数番目のサンプルを分け、それぞれの DFT を再帰的に求める代わりに、末尾再帰をループで展開した **反復型 in-place 実装** を採用。
- **効果**: 乗算回数が $N^2$ → $\frac{N}{2} \log_2 N$ に減少。N=1024 では比例定数を無視しても 1024 vs 10 回（100倍レベル）で済み、実測でも **845 倍** の高速化を実現。

#### 2.4.2 ビット逆順ソートによる in-place 化
- **工夫**: 入力配列を「インデックスのビット逆順」に並べ替えた上でバタフライ演算を行うことで、作業用配列を一切使わない in-place 実装とした。
- **効果**: メモリ使用量が $O(N)$ で一定（作業配列ゼロ）。キャッシュ効率も高くなり、特に N が大きい場合に顕著に寄与する。N=8192 で **5100 倍** の高速化を達成。

#### 2.4.3 回転子 $W_N^{kn}$ の漸次計算
- **工夫**: 各 `len` について `W_N^{kn}` を `cexp()` で毎回計算するのではなく、初期値 `w = 1` から `w *= wlen` で 1 ステップずつ更新。
- **効果**: `cexp()` 呼び出しが $O(N \log N)$ 回 → $O(N)$ 回に減少。`cexp()` は `cos` と `sin` を評価する重い関数で、ループ内で回避できる点が大きい。複素数乗算の累算にも同じ `w` 変数を使い回すことで、レジスタに乗りやすいコードになっている。

#### 2.4.4 FFT と IFFT のコード共通化
- **工夫**: `Butterfly()` 1 つに `sign = ±1` の引数を持たせ、FFT (sign=-1) と IFFT (sign=+1) を共通化。IFFT は最後に $1/N$ するだけ。
- **効果**: コード量が半減し、バグが入る余地を最小化。DFT/IDFT との比較も `sign` を変えるだけで実行できる。

#### 2.4.5 結果の数値検証の組込み
- **工夫**: same入力に対して DFT と FFT を両方走らせ、`|Xd[k] - Xf[k]|` の最大値 と `|IFFT(X) - x|` の最大値 を `stderr` に出力。
- **効果**: 高速化の「正しさ」を自動検証。N=1024 ではスペクトル誤差 `1.99e-11`、波形復元誤差 `1.19e-14` となり、FFT が DFT と数学的に等価であることを誤差レベルで証明できる。レポートに「速くなったが結果が違う」式のバグを混入させない自検査機構。

#### 2.4.6 `FloorPow2()` による `N` の自動切り下げ
- **工夫**: 標本数 `N` が 2 のべき乗でない場合は、超えない最大の 2 のべき乗に自動的に切り下げる。
- **効果**: radix-2 FFT の制約（N が 2 のべき乗であること）をユーザが意識しなくても済み、誤動作を未然に防止。

#### 2.4.7 高精度タイマーによる定量的比較
- **工夫**: `clock_gettime(CLOCK_REALTIME, ...)` でナノ秒精度のタイマーを用意し、DFT と FFT を同一プロセス内で同一入力に対して順次計測。
- **効果**: 温度スロットリング等の環境変動を同一プロセス内で打ち消し、純粋なアルゴリズムの差を定量化できた。N を 64 から 8192 まで振ったロググラフ（`ft_timing.png`）で、計算量オーダーが $O(N^2)$ から $O(N \log N)$ へ変化したことまで可視化できている。

---

## 3. DFT・FFT に対する感想・考察

### 3.1 速さの体感：845 倍という数字の重み

N=1024 という「たった千サンプル」の DFT で **75 ms** も掛かっていた処理が、FFT では **89 µs** に縮んだ。これは単なる数字ではなく、音声 1 フレーム（21 ms 相当）のスペクトル分析を **DFT ではリアルタイムに追いつかないが、FFT なら余裕で間に合う** という質的な差に直結する。DFT は「理論としての周波数変換」、FFT は「実用的な道具」という関係が、体感として初めて腑に落ちた。音声信号を 48 kHz でリアルタイム処理する場合、1 サンプルあたり 21 µs しか予算がない。FFT がなければ、今日の音声認識・ノイズキャンセリング・イコライザといった技術の多くは成立しないはずだ。

### 3.2 「美しいアルゴリズム」の発見

本課題を通じて最も印象的だったのは、Cooley–Tukey のアルゴリズムがただの高速化手法ではなく、**DFT の定義式に隠れていた対称性を抽出したもの** だという点である：

```
X[k]     = E[k]   + W_N^k  O[k]
X[k+N/2] = E[k]   - W_N^k  O[k]
```

偶数項 `E[k]` と奇数項 `O[k]` が、回転子 `W_N^k` を挟んだ対称な形で 2 つの異なるスペクトル点を同時に与える、この「バタフライ」の形は、数学的な**冗長性を直接コードの構造に落とし込んだ** ものだと気づいたとき、単なるテクニックではなく **「式の構造を再利用する」という思想** が見えた気がした。Haskell や Rust の再帰と違って、C の `for` 文と `while` だけでこれを in-place 表現できるのも面白かった。ビット逆順ソートの 1 ステップで `i ^= bit` を累積する書き方などは、簡潔だが一見では理解できない美しさがある。

### 3.3 計算量の可視化：`O(N²)` vs `O(N log N)` の肌感

`ft_timing.png` の両対数グラフを見ると、DFT 側は傾き 2 の直線、FFT 側は `log N` による緩やかな曲線に乗っている。N を倍にしたとき DFT は 4 倍、FFT は 2 倍強、にしかならないという**漸近的圧倒** が、数式上の議論ではなく肌で分かる。課題の身近な例でも「N=8192 では DFT は 5 秒も掛かるが FFT は 1 ms 未満」だ。5 秒の遅延はライブ演奏では致命的だが、1 ms なら全く問題ない。理論と実用の距離が、たった 1箇所のアルゴリズム変更で埋まるという感動があった。

### 3.4 「正しさ」の検証：丸め誤差との付き合い

高速化と並んで勉強になったのが、`|Xd[k] - Xf[k]| ≈ 10^{-11}` という極小だがゼロでない差である。FFT は DFT と**数学的には等価** だが、計算順序が違うために浮動小数点の累積誤差が異なる。特に回転子を `w *= wlen` で漸次的に更新する実装では、`cexp()` を毎回呼ぶ DFT に比べて誤差がやや大きい傾向があった。しかし 10^{-11} は `double` 精度（約 16 桁）の範囲内であり、実用上は全く問題ない。むしろ、ここまでの差しか出ないということは、**IEEE 754 倍精度の枠内で典型的なオーディオ処理には十分** であるという確証が得られた。もし医学画像や長時間相関の解析などで誤差が問題になるなら、Kahan の補正付き加算や `long double` / `__float128` の採用を検討する必要があるだろう。

### 3.5 DFT から FFT へ：学問史への敬意

1965 年に Cooley と Tukey が FFT を発表してから、デジタル信号処理の応用範囲が爆発的に広がったと聞く。それまでは DFT の $O(N^2)$ が「手で計算できる限界」にあり、周波数分析は実験室の中だけの技術だった。本課題で DFT と FFT を同時に実装して比較することで、60 年前に彼らが見出した単純な再帰分解が、**理論と実用を結ぶ境界線** だったという歴史的意味を初めて実感した。离散 Fourier 変換という 200 年前の数学が、畳み込み定理と FFT を経て今のスマホのノイズキャンセリングにまで繋がっているという連続性に、工学的感動と共に、その道を切り開いた先人たちへの敬意を覚えた。

### 3.6 残された課題と今後の展望

本実装は基数 2 に限っているため、N が 2 のべき乗でない場合は切り下げが必要となる。次は：

1. **Bluestein / chirp-z 変換**: 任意の N で $O(N \log N)$ で DFT する。
2. **混合基数 (mixed-radix) FFT**: $N = 2^a \cdot 3^b \cdot 5^c$ などにも対応。
3. **real FFT**: 実信号用にhalf-spectrum だけ計算し、計算量を半減。
4. **SIMD 化**: AVX / ARM NEON でバタフライを並列化。
5. **キャッシュ対応**: N が大きい場合は 6–8 段でブロック分割しタイル化（cache-friendly FFT）。

これらを実装すれば、さらに 2～10 倍の高速化余地 が見込める。今回の `ft.c` は FFT の「最低限動く版」だが、その中にも $O(N \log N)$ の思想が息づいており、これを足場に次の最適化に進みたい。

---

*以上*