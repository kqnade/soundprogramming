# 信号処理 課題レポート集

レポート、ソース、解析結果、音源、提出用PDFをこのフォルダに収録している。各レポートのコード付録は `pcm/` の実ファイルと一致する。

## レポート一覧

| 対象資料・課題 | 本文 | PDF |
|---|---|---|
| Wk1・Wk9：標本化定理、2タップFIRの練習問題 | [練習問題](report_exercises.md) | [PDF](pdf/report_exercises.pdf) |
| Wk2：WAV属性、ヘッダ、入出力、再生速度 | [PCM・WAV](report_pcm.md) | [PDF](pdf/report_pcm.pdf) |
| Wk3：音源合成、seq.c、mix.c（sp-2704） | [音源生成](report_generation.md) | [PDF](pdf/report_generation.pdf) |
| Wk4・Wk5：DFT・IDFT（sp-1805） | [DFT](report_dft.md) | [PDF](pdf/report_dft.pdf) |
| Wk6・課題画像：DFTからFFT（sp-0106） | [FFT](report_fft.md) | [PDF](pdf/report_fft.pdf) |
| Wk8・Wk10：フィルタの理論 | [フィルタの基礎](report_analog_filter.md) | [PDF](pdf/report_analog_filter.pdf) |
| Wk11：IIR・FIR実装、IRと周波数特性 | [フィルタ実験](report_filter.md) | [PDF](pdf/report_filter.pdf) |
| Wk12・Wk13：変調・シンセサイザーエフェクト（sp-0709） | [シンセサイザー](report_synth.md) | [PDF](pdf/report_synth.pdf) |

ページ番号の根拠：Wk1の21ページ、Wk2の14〜16ページ、Wk3の18〜23ページ、Wk5の19ページ、Wk9の28〜29ページ、Wk11の7〜14ページ、Wk13の22ページ。FFTの提出指示は `../slide/課題_sp0106.jpg` にある。

Wk4・Wk6・Wk8・Wk12の末尾には独立した提出指示はなく、関連するレポートに内容を含めた。`Wk10-DigitalFilter2.pptx` の表示スライドは1〜27枚目で、非表示の75枚目にあるDFT課題はDFTレポートと内容が重なる。非表示の別テーマ資料は信号処理の課題に含めていない。

## ファイル構成

```text
reports/
  report_*.md          レポート本文・コード付録
  pdf/                 本文と図を含むPDF
  pcm/                 Cソース、共通ヘッダ、Makefile
  assets/              本文で使用する測定値、ログ、図、WAV
    results.json       全測定値、環境、ソースのSHA-256
    commands.log       実行コマンドと標準エラー出力
    fourier/           DFT/FFTの数値・図・スクリーンショット
    synth/             エフェクトのWAV・図・スクリーンショット
  reproduce.py         全実験とグラフの再生成
  analyze_audio.py     16 bit・モノラルWAVの測定
  tests/               コード・レポートの整合性テスト
  reference/           保存音源と参考ファイル（本文の測定根拠とは別）
```

授業スライドはリポジトリの `slide/` にある。

## ビルドと検証

動作確認環境はLinux x86-64、Python 3.14、GCC 16.1.1。Cコードには、リトルエンディアン・通常の32 bit整数/64 bit doubleを前提とする授業用PCMライブラリを使用している。

必要なもの：Cコンパイラ、GNU Make、`hexdump`、Python。音声処理用のFFmpegはPATH上の実行ファイルを優先し、なければ `imageio-ffmpeg` の同梱版を使用する。

リポジトリのルートで実行する。

```sh
python3 -m venv reports/.venv
reports/.venv/bin/pip install -r reports/requirements.txt
make -C reports/pcm

PYTHONDONTWRITEBYTECODE=1 reports/.venv/bin/python \
  -m unittest discover -s reports/tests -p 'test_*.py'
```

実行ファイルは `reports/pcm/build/` に生成される。共有ライブラリは同じディレクトリから読み込むため、`LD_LIBRARY_PATH` の設定は不要。`build/` は生成物としてGitの対象外とする。

テストは既知の正弦波、無音、WAV属性、引数の拒否、連結・混合条件、FIR利得、IIR帰還係数、FFTの入力保持、全実験の再現、掲載コード・リンク・測定ソースの整合性を確認する。全実験テストの出力先は一時ディレクトリであり、本文に添付した測定結果は上書きしない。

## 全実験の再実行

```sh
reports/.venv/bin/python reports/reproduce.py
```

`assets/` 内の実験結果を上書きする。別の測定を比較したい場合は `--output /tmp/sound-experiment` を指定する。

`commands.log` の `$BIN` は `reports/pcm/build/`、`$ASSETS` は測定出力ディレクトリ、`$WORK` はテキスト出力用の一時ディレクトリを表す。

計算時間は環境・負荷で変動するため、再測定した値を採用する場合は本文の数値・表・考察も照合する。測定時のソースと本文のタイミング表はテストで照合し、不一致を検出する。

Cアプリ `dft`・`fft`・`ft` を単独実行すると、波形とスペクトルのテキストがカレントディレクトリへ出力される。既存データと混在させないため、一時ディレクトリでの実行を推奨する。

## 図・スクリーンショット

- FFT：[結果表示ページ](assets/fourier/view.html)、[スクリーンショット](assets/fourier/screenshot.png)
- シンセサイザー：[結果表示ページ](assets/synth/view.html)、[スクリーンショット](assets/synth/screenshot.png)

スクリーンショットは、解析PNGを表示するHTMLページをChromiumで描画して取得したもの。WAVの解析値は `results.json`、画像表示ページには外部ネットワーク資源を使用していない。

再撮影にはPlaywrightとChromiumを利用できる。

```sh
reports/.venv/bin/pip install playwright
reports/.venv/bin/python -m playwright install chromium --only-shell
for name in fourier synth; do
  reports/.venv/bin/python -m playwright screenshot --full-page \
    --viewport-size='1280,900' --wait-for-timeout=1000 \
    "file://$PWD/reports/assets/$name/view.html" \
    "reports/assets/$name/screenshot.png"
done
```

## PDF

PDFはMarkdown本文から作成し、数式・グラフ・ソース付録を含む。再生成にはPandoc、XeLaTeX、Noto Sans CJK JP、Noto Sans Mono CJK JPを使用する。

```sh
cd reports
for file in report_*.md; do
  pandoc "$file" -f markdown+tex_math_single_backslash --standalone --pdf-engine=xelatex \
    -V documentclass=article -V CJKmainfont='Noto Sans CJK JP' \
    -V mainfont='Noto Sans CJK JP' -V monofont='Noto Sans Mono CJK JP' -V fontsize=10pt \
    -V papersize=a4 -V geometry:margin=20mm --include-in-header=pdf-header.tex \
    --lua-filter=pdf-links.lua \
    -o "pdf/${file%.md}.pdf"
done

python3 - <<'PY'
import hashlib, json
from pathlib import Path
entries = {}
for pdf in sorted(Path('pdf').glob('report_*.pdf')):
    md = Path(pdf.stem + '.md')
    entries[pdf.name] = {
        'markdown_sha256': hashlib.sha256(md.read_bytes()).hexdigest(),
        'pdf_sha256': hashlib.sha256(pdf.read_bytes()).hexdigest(),
    }
Path('pdf/manifest.json').write_text(json.dumps(entries, indent=2) + '\n')
PY
```

## 評価の範囲

記載した実験値は、添付ソースと音源に基づく実際の数値・画像評価である。試聴による音質評価は含まない。Wk11の聴き比べに使うIIR/FIR出力、および各エフェクトの出力WAVは添付している。採点結果や、試験していない環境・入力での動作を保証するものではない。
