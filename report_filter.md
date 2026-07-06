# ディジタルフィルタ３ レポート

**実装項目**

- `iir.c`：IIR 簡易リバーブフィルタ
- `fir.c`：インパルス応答を利用した FIR フィルタ
- `ip.c`：インパルス信号生成（IIR のインパルス応答取得用）
- `fft.c`：FFT による周波数特性解析ツール

---

## 1. 概要

本課題では，FIR 方式および IIR 方式のディジタルフィルタを libpcm.so を用いて実装し，リバーブ効果の付与とその周波数特性の確認を行う．

- **IIR フィルタ**：少数の遅延帰還経路で残響・反響効果を生成．メモリと演算量が少ないが，安定性に注意が必要．
- **FIR フィルタ**：IIR フィルタのインパルス応答を畳み込み係数として利用し，同じ周波数特性を再現．実装は単純だが，長いインパルス応答に対して演算コストが大きい．
- **FFT**：インパルス応答のスペクトルを計算し，フィルタの伝達関数を観察する．

---

## 2. IIR 簡易リバーブフィルタ (`iir.c`)

### 2.1 設計方針

並列帰還型の IIR フィルタを採用した．

```
y[n] = b0 * x[n] + Σ a[m] * y[n - d[m]]
```

- 入力ゲイン `b0 = 0.5`：音割れ防止のため入力振幅を半減．
- 帰還遅延 `d[m]`：反響の距離感を出すため，fs = 48 kHz で数十 ms 程度となるよう設定．
  - 1009 標本（約 21 ms）
  - 2011 標本（約 42 ms）
  - 3001 標本（約 63 ms）
- 帰還係数 `a[m]`：1.0 未満に抑え，さらに総和が 1.0 未満となるように選定して安定性を確保．
  - 0.40, 0.30, 0.20（総和 0.90）

遅延量は互いに素な値を選ぶことで，帰還信号同士の干渉による濁りを軽減している．

### 2.2 ソースコード

```c
// IIR 簡易リバーブフィルタ
// Ver.2026.07.06
// コンパイル：$ cc iir.c -std=c99 -I. -L. -lpcm -lm -o iir
// 実行例：$ cat input.wav | ./iir | paplay
// 　　　　$ cat input.wav | ./iir > output.wav

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcm.h"

#define debug(...)      fprintf(stderr, __VA_ARGS__)
#define fatal(s, ...)   { debug(__VA_ARGS__); exit(s); }

static const double b0 = 0.5;
static const int    delay[] = { 1009, 2011, 3001 };
static const double alpha[] = { 0.40, 0.30, 0.20 };
static const int    N_DELAY = sizeof(delay) / sizeof(delay[0]);

int main(int argc, char *argv[])
{
    Wav *p = pcmRead(stdin);
    if (p == NULL) fatal(1, "入力WAVの読込失敗\n");

    debug("■ IIR 簡易リバーブフィルタ\n");
    pcmInfo(stderr, p);

    int dmax = 0;
    for (int i = 0; i < N_DELAY; i++) {
        if (delay[i] > dmax) dmax = delay[i];
        debug("帰還経路 %d：遅延 = %d 標本，係数 = %f\n",
            i + 1, delay[i], alpha[i]);
    }
    debug("入力ゲイン b0 = %f\n", b0);
    debug("最大遅延量 = %d 標本（約 %.3f [s]）\n",
        dmax, (double)dmax / p->fmt.fs);
    debug("\n");

    for (int c = 0; c < p->fmt.ch; c++) {
        unsigned int len = p->len;
        double *x = p->val[c];
        double *y = (double *)calloc(len + dmax, sizeof(double));
        if (y == NULL) fatal(1, "出力バッファ確保失敗\n");

        for (unsigned int n = 0; n < len; n++) {
            double s = b0 * x[n];
            for (int i = 0; i < N_DELAY; i++) {
                s += alpha[i] * y[n + dmax - delay[i]];
            }
            y[n + dmax] = s;
        }
        for (unsigned int n = 0; n < len; n++) {
            x[n] = y[n + dmax];
        }
        free(y);
    }

    pcmWrite(stdout, p);
    pcmFin(p);
    return (0);
}
```

---

## 3. インパルス応答の取得 (`ip.c` + `iir`)

### 3.1 インパルス信号の生成

`ip.c` は先頭 1 標本に振幅 0.9 のインパルスを与え，残りを無音とした WAV を生成する．音長は 2.0 秒（96,000 標本）とし，IIR の帰還成分が十分に収まる長さを確保した．

```c
p->val[0][0] = 0.9;
```

### 3.2 インパルス応答の生成

```bash
$ ./ip > ip.wav
$ cat ip.wav | ./iir > ir.wav
```

`ir.wav` が IIR フィルタのインパルス応答となる．

---

## 4. FIR フィルタ (`fir.c`)

### 4.1 設計方針

`ir.wav` の標本値列を FIR 係数 `b[m]` として読み込み，入力信号との畳み込みによってフィルタリングを行う．

```
y[n] = Σ b[m] * x[n - m]
```

- 入力信号を `nb - 1` 標本延長し，畳み込みの終端効果を正しく出力する．
- 音割れ防止のため，係数の絶対値和で正規化した．
  - `norm = 1.0 / Σ |b[m]|`
  - これにより，入力振幅が最大値でも出力が概ね [-1, +1] に収まる．

### 4.2 工夫点：畳み込みの高速化

二重ループを

```c
for (int m = 0; m < nb; m++) {
    for (int n = 0; n < len0; n++) {
        y[n + m] += b[m] * x[n] * norm;
    }
}
```

と書くことで，範囲外アクセス判定を内側に入れる必要がなく，分岐を削減している．

ただし，FIR はタップ数 `nb` に対して O(len0 * nb) の計算量となる．本実装では 2 秒分のインパルス応答（96,000 タップ）をそのまま用いているため，1 秒入力に対して約 4.6 × 10^9 回の乗算が発生し，リアルタイム処理には不向きである．実用的にはインパルス応答の末尾を閾値以下で打ち切るか，FFT を用いた高速畳み込み（fast convolution）が必要となる．

### 4.3 ソースコード

```c
// FIR フィルタ（インパルス応答による畳み込み）
// Ver.2026.07.06
// コンパイル：$ cc fir.c -std=c99 -I. -L. -lpcm -lm -o fir
// 実行例：$ cat input.wav | ./fir ir.wav | paplay

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcm.h"

#define debug(...)      fprintf(stderr, __VA_ARGS__)
#define fatal(s, ...)   { debug(__VA_ARGS__); exit(s); }

int main(int argc, char *argv[])
{
    char *fn = "ir.wav";
    if (argc > 1) fn = argv[1];
    Wav *ir = pcmLoad(fn);
    if (ir == NULL) fatal(1, "ir: Read失敗\n");

    debug("■ FIR フィルタ\n");
    debug("インパルス応答ファイル = %s\n", fn);
    pcmInfo(stderr, ir);

    double *b = ir->val[0];
    int nb = ir->len;

    Wav *p = pcmRead(stdin);
    if (p == NULL) fatal(1, "入力WAVの読込失敗\n");

    int len0 = p->len;
    int dn = nb - 1;
    int len = len0 + dn;

    Wav *out = pcmInit(p->fmt.bit, p->fmt.ch, p->fmt.fs, len);
    if (out == NULL) fatal(1, "出力用WAV確保失敗\n");

    double sumAbs = 0.0;
    for (int m = 0; m < nb; m++) sumAbs += fabs(b[m]);
    double norm = (sumAbs > 0.0) ? 1.0 / sumAbs : 1.0;
    debug("インパルス応答の絶対値和 = %f，正規化係数 = %f\n", sumAbs, norm);

    for (int c = 0; c < p->fmt.ch; c++) {
        double *x = p->val[c];
        double *y = out->val[c];
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
```

---

## 5. FFT による周波数特性解析 (`fft.c`)

`dft.c` を元に，処理時間測定・DFT/IDFT 呼出を削除し，FFT のみを呼び出すようにした．入力波形を `wf.txt`，振幅スペクトルを `fft.txt` に出力する．

Cooley-Tukey 方式の radix-2 FFT を採用し，標本数は 2 のべき乗に切り下げる．

```bash
$ ./fft ir.wav 8192 0
$ ./fft input.wav 8192 0
```

インパルス応答のスペクトルはフィルタの伝達関数 `H[k]` に対応する．

---

## 6. 実験手順と結果

### 6.1 コンパイル

```bash
$ make
```

`libpcm.so` とともに `iir`, `ip`, `fft`, `fir` が生成される．

### 6.2 インパルス応答の取得

```bash
$ ./ip > ip.wav
$ cat ip.wav | ./iir > ir.wav
```

生成された `ir.wav` の属性：

| 項目 | 値 |
|------|-----|
| チャネル数 | 1 |
| 標本化周波数 | 48,000 Hz |
| 標本数 | 96,000 |
| 収録時間 | 2.0 s |

### 6.3 FIR フィルタの適用例

```bash
$ ./sin 440 16 > sin440.wav
$ cat sin440.wav | ./fir ir.wav > fir_out.wav
```

入力 48,000 標本に対し，出力は 48,000 + 96,000 - 1 = 143,999 標本となり，畳み込みによる延長が確認できた．

### 6.4 周波数特性の確認

```bash
$ ./fft ir.wav 8192 0
```

`fft.txt` において，複数のピークが櫛状に現れ，コムフィルタ的な周波数応答が観察できた．これは並列帰還型 IIR フィルタの特徴である．

---

## 7. 工夫点および性能向上への効果

### 7.1 IIR 側の工夫

1. **安定性の確保**：帰還係数の総和を 1.0 未満（0.90）に抑え，Rouché の定理に基づく安定性を保証した．
2. **遅延量の選定**：互いに素な値（1009, 2011, 3001）を用い，帰還信号の干渉による濁りを抑制した．
3. **入力ゲインの調整**：`b0 = 0.5` とすることで，入力側の即時音と帰還音の合成による音割れを軽減した．

### 7.2 FIR 側の工夫

1. **音割れ防止の正規化**：係数の絶対値和で割ることで，最大振幅が概ね 1.0 以内に収まるようにした．
2. **内側ループの範囲外判定排除**：畳み込みの走査順を工夫し，分岐を減らして実行効率を高めた．
3. **延長領域の明示的確保**：出力を `len0 + nb - 1` 標本確保し，畳み込みの終端部も欠落させないようにした．

### 7.3 さらなる性能向上策

- **インパルス応答の打ち切り**：小振幅部分を閾値で切り捨て，タップ数を減らす．
- **FFT 高速畳み込み**：長い FIR に対して O(N log N) の高速畳み込みを適用する．
- **ステレオ処理の最適化**：現状は各チャネル独立だが，モノラル IR を使う場合は計算を共通化できる．

---

## 8. FIR・IIR に対する感想・考察

### 8.1 IIR の利点・欠点

**利点**

- 長い残響を少数の係数（遅延と帰還係数）で表現でき，メモリ効率が非常に高い．
- 演算量が入力標本数に対してほぼ比例し，リアルタイム処理に適している．

**欠点**

- 帰還係数や遅延量の選定によっては容易に発散・発振するため，安定性解析が必要．
- 周波数特性が直感的でなく，コムフィルタ特有の coloring 効果を避けるのが難しい．

### 8.2 FIR の利点・欠点

**利点**

- 実装が単純で，任意の LTI システムをインパルス応答さえあれば再現できる．
- 常に安定（有限のインパルス応答を持つため）．

**欠点**

- 長い残響を再現するにはタップ数が膨大になり，メモリと演算量が急増する．
- 本実装では 1 秒入力に対して数秒の FIR 処理時間がかかり，リアルタイム処理には不向き．

### 8.3 総合考察

IIR は「効率的に残響を生成する」ことに長け，FIR は「既存フィルタの特性を忠実に再現する」ことに長けている．本課題では IIR でリバーブを設計し，そのインパルス応答を FIR に移植することで，両者の関係性を確認できた．

リバーブのような長時間効果を実用的に実装する場合，IIR が有力であり，FIR は短い IR や FFT 高速畳み込みと組み合わせて使うのが現実的である．また，IIR 設計では安定性を犠牲にせずに係数を大きくする方法（all-pass フィルタ，Schroeder リバーブなど）が今後の課題として挙げられる．

---

## 9. 付録：実行コマンド一覧

```bash
# ライブラリとアプリのビルド
$ make

# インパルスとインパルス応答の生成
$ ./ip > ip.wav
$ cat ip.wav | ./iir > ir.wav

# IIR 処理の適用
$ cat input.wav | ./iir | paplay

# FIR 処理の適用
$ cat input.wav | ./fir ir.wav | paplay

# 周波数特性の確認
$ ./fft ir.wav 8192 0
$ ./dw.plot wf.txt      # 波形表示（別途 plot スクリプトがあれば）
$ ./ds.plot fft.txt     # スペクトル表示（別途 plot スクリプトがあれば）
```
