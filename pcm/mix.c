// WAVファイル混合アプリ（ミキサー）
// コンパイル：$ cc mix.c -std=c99 -I. -L. -lpcm -o mix
// 実行例：$ ./mix C.wav E.wav G.wav > CEG.wav
// 　　　　$ ./mix C.wav E.wav G.wav | paplay

#include <stdio.h>
#include <stdlib.h>
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

	// 全ファイルを読み込み，最大標本数を取得
	unsigned int	max_len = 0;
	unsigned int	bit = 16, ch = 1, fs = 48000;

	for (int i = 0; i < n; i++) {
		in[i] = pcmLoad(argv[i + 1]);
		if (in[i] == NULL)
			fatal(1, "読み込み失敗：%s\n", argv[i + 1]);
		if (in[i]->len > max_len) max_len = in[i]->len;
		if (i == 0) {	// 最初のファイルのPCM属性を出力に使う
			bit = in[i]->fmt.bit;
			ch  = in[i]->fmt.ch;
			fs  = in[i]->fmt.fs;
		}
	}

	// 出力用 WAV を作成（容器：最大標本数分）
	Wav	*out = pcmInit(bit, ch, fs, max_len);
	if (out == NULL) fatal(1, "出力用WAV確保失敗\n");

	// 各チャネルの瞬時値を混合
	// 音割れ対策：入力数 n で割って振幅を正規化
	for (int c = 0; c < (int)ch; c++) {
		for (unsigned int j = 0; j < max_len; j++) {
			double	v = 0.0;
			for (int i = 0; i < n; i++) {
				if (j < in[i]->len)
					v += in[i]->val[c][j];
			}
			out->val[c][j] = v / n;
		}
	}

	pcmWrite(stdout, out);

	for (int i = 0; i < n; i++) pcmFin(in[i]);
	pcmFin(out);
	free(in);
	return (0);
}
