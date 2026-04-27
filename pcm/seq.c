// WAVファイル連結アプリ（シーケンサー）
// コンパイル：$ cc seq.c -std=c99 -I. -L. -lpcm -o seq
// 実行例：$ ./seq C.wav D.wav E.wav > C-D-E.wav
// 　　　　$ ./seq C.wav D.wav E.wav | paplay

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pcm.h"

#define	debug(...)	fprintf(stderr, __VA_ARGS__)
#define	fatal(s, ...)	{ debug(__VA_ARGS__); exit(s); }

int main(int argc, char *argv[])
{
	if (argc < 2)
		fatal(1, "使い方：%s file1.wav file2.wav ...\n", argv[0]);

	int	n = argc - 1;		// 入力ファイル数

	// 入力 WAV を格納するポインタ配列
	Wav	**in = malloc(n * sizeof(Wav *));
	if (in == NULL) fatal(1, "メモリ確保失敗\n");

	// 全ファイルを読み込み，総標本数を計算
	unsigned int	total_len = 0;
	unsigned int	bit = 16, ch = 1, fs = 48000;

	for (int i = 0; i < n; i++) {
		in[i] = pcmLoad(argv[i + 1]);
		if (in[i] == NULL)
			fatal(1, "読み込み失敗：%s\n", argv[i + 1]);
		total_len += in[i]->len;
		if (i == 0) {	// 最初のファイルのPCM属性を出力に使う
			bit = in[i]->fmt.bit;
			ch  = in[i]->fmt.ch;
			fs  = in[i]->fmt.fs;
		}
	}

	// 出力用 WAV を作成（容器：総標本数分）
	Wav	*out = pcmInit(bit, ch, fs, total_len);
	if (out == NULL) fatal(1, "出力用WAV確保失敗\n");

	// 各入力の標本値を出力配列へ順番にコピー
	unsigned int	offset = 0;
	for (int i = 0; i < n; i++) {
		for (int c = 0; c < (int)ch; c++) {
			memcpy(out->val[c] + offset,
			       in[i]->val[c],
			       in[i]->len * sizeof(double));
		}
		offset += in[i]->len;
	}

	pcmWrite(stdout, out);

	for (int i = 0; i < n; i++) pcmFin(in[i]);
	pcmFin(out);
	free(in);
	return (0);
}
