/*
 * libpcm.so - PCM用WAVファイル入出力ライブラリ
 * Version：2023.12.08
 * 著者：yanagawa@kushiro-ct.ac.jp
 *
 * 想定データの形式：非圧縮リニアPCM，8*nビット，1or2チャンネル
 * 想定WAVファイルの構成：｛RIFFヘッダ，...，fmtチャンク，dataチャンク，...｝
 * 	# 「...」部分は無視
 * 想定システムのバイトオーダ：リトルエンディアン
 * 	# RIFFファイル内の数値データはリトルエンディアンなので，
 * 	# Intel製CPU等のリトルエンディアンなシステムでは取り扱いが容易．
 * 	# ビッグエンディアンなシステムではバイトオーダ変換処理が必要．
 * ソースコードの言語標準：C99
 * ライブラリ関数の戻り値の流儀：0=NULL=エラー
 *
 * ライブラリのコンパイル方法（GCC）：$ cc -c pcm.c -std=c99 -fpic -shared -o libpcm.so
 * アプリのコンパイル方法（GCC）：$ cc ソース.c -lpcm -o プログラム
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "pcm.h"

#define debug(...)	fprintf(stderr, __VA_ARGS__)
#define fatal(err, ...)	{ debug(__VA_ARGS__); exit(err); }

// ファイルからnバイトデータ列の取得
static unsigned int readData(FILE *fp, unsigned int n, void *data)
{
	if (!fp) return (0);
	if (!data) return (0);

	if (fread(data, 1, n, fp) < n) return (0);
	return (n);
}

// ファイルからサイズ値（符号なし整数）の取得
static unsigned int readSize(FILE *fp)
{
	unsigned int	size;
	if (fread(&size, 4, 1, fp) < 1) return (0);
	return (size);
}

// ファイルからチャンクIDの照合
static int isChunk(FILE *fp, const char *id)
{
	int	n = strlen(id);
	if (n != 4) return (0);

	char	buf[4];		// ID文字配列
	if (fread(buf, 1, 4, fp) < 4) return (0);
	if (strncmp(buf, id, 4) != 0) return (0);
		// 注意：bufは，文字の配列ではあるが，文字列ではない
		// （固定長４文字，終端記号なし）
	return (1);
}

// ファイルからチャンクIDの検索
static int seekChunk(FILE *fp, const char *id)
{
	while (!isChunk(fp, id)) {	// 別のチャンクにいる場合...
		unsigned int	n = readSize(fp);
		if (n == 0) return (0);

		// 次のチャンクまでスキップ
		if (fseek(fp, (long)n, SEEK_CUR) != 0) return (0);
	}
	return (1);
}

// ファイルへのnバイトデータ列の書込
static int writeData(FILE *fp, int n, const void *data)
{
	if (!fp) return (0);
	if (n < 0) return (0);
	if (fwrite(data, 1, n, fp) < n) return (0);
	return (n);
}

// バイナリデータと実数値の相互変換用定数配列
static const unsigned int	DatMax[] = {0, 0xFF, 0x7FFF, 0x7FFFFF, 0x7FFFFFFF};	// 最大値
static const unsigned int	DatMin[] = {0, 0x00, 0x8000, 0x800000, 0x80000000};	// 最小値（補数表現された負値）
static const double		DatAmp[] = {0, 0x80, 0x8000, 0x800000, 0x80000000};	// 振幅の最大値

// n=[0, 4]バイトバイナリデータから実数値への変換
static double dat2val(unsigned char *dat, unsigned int n)
{
	int	bin = 0x00000000;	// ４バイト分のバッファ
	memcpy(&bin, dat, n);		// datから先頭nバイト分をbinにコピー
		// リトルエンディアンなので下位nバイト分ですよ．
	double	val = (double)bin/DatAmp[n];
	if (n == 1) val -= 1.0;			// 8bit→ かさ下げ -128
	else if (val >= 1.0) val -= 2.0;	// 補数表現→ 負値
	return (val);
}

// 実数値からn=[0, 4]バイトバイナリデータへの変換
static void val2dat(unsigned char *dat, double val, unsigned int n)
{
	unsigned int	bin;
	if (val <= -1.0) {
		bin = DatMin[n];	// 最小値制限
	} else if (val >= 1.0) {
		bin = DatMax[n];	// 最大値制限
	} else {
		if (n == 1) val += 1.0;		// 8bit→ かさ上げ +128
		else if (val < 0.0) val += 2.0;	// 負値→ 補数表現
		bin = (unsigned int)(val*DatAmp[n]);
	}
	memcpy(dat, &bin, n);
}

// Wav構造体の生成
static Wav *newWav(const Fmt *fmt)
{
	if (!fmt) return (NULL);

	Wav	*p = (Wav *)malloc(sizeof(Wav));
	if (!p) return (NULL);

	p->fmtSize = sizeof(Fmt);
	p->fmt = *fmt;
	return (p);
}

// Wav構造体の破棄
static void freeWav(Wav *p)
{
	if (!p) return;
	if (p->data) free(p->data);
	for (int c = 0; c < p->fmt.ch; c++) {
		if (p->val[c]) free(p->val[c]);
	}
	free(p);
}

// PCM用Wav構造体の生成・初期化
Wav *pcmInit(unsigned int bit, unsigned int ch, unsigned int fs, unsigned int len)
{
	if (bit%8 != 0) return (NULL);	// 8*n[bit] only
	if (!((ch == 1) || (ch == 2))) return (NULL);
	if (fs == 0) return (NULL);
	if (len == 0) return (NULL);

	// fmtチャンク部分の初期化
	int	bs = (bit/8)*ch;
	Fmt	fmt = {1, ch, fs, fs*bs, bs, bit};

	// Wav構造体の生成
	Wav	*p = newWav(&fmt);
	if (!p) return (NULL);

	// dataチャンク部分の初期化
	unsigned int	size = bs*len;
	p->size = size + size%2;	// 偶数化 i.e. if (size%2) p->size = size + 1; 
	p->data = (unsigned char *)calloc(bs, len);
	if (!p->data) goto ERROR;

	// 独自拡張メンバの初期化
	p->len = len;
	p->time = (double)len/(double)fs;
	p->byte = bs/ch;
	for (int c = 0; c < ch; c++) {
		p->val[c] = (double *)calloc(sizeof(double), len);
		if (!p->val[c]) goto ERROR;
	}
	return (p);
ERROR:
	freeWav(p);
	return (NULL);
}

// PCM用Wav構造体の破棄
void pcmFin(Wav *p)
{
	freeWav(p);
}

// WAVファイルからPCMデータの生成
Wav *pcmRead(FILE *fp)
{
	if (!fp) return (NULL);

	// RIFFヘッダの入力
	if (!isChunk(fp, "RIFF")) return (NULL);	// not RIFFファイル
	readSize(fp);	// RIFFチャンクサイズは読み飛ばしてOK
	if (!isChunk(fp, "WAVE")) return (NULL);	// not WAV

	// fmtチャンクの入力
	if (!seekChunk(fp, "fmt ")) return (NULL);
	unsigned int	fmtSize = readSize(fp);
	if (fmtSize != 16) return (NULL);	// not 非圧縮PCM

	Fmt	fmt;
	readData(fp, 16, &fmt);
	if (fmt.type != 1) return (NULL);	// not リニアPCM
	if ((fmt.bit)%8 != 0) return (NULL);	// not 8*nビット

	// Wav構造体の生成
	Wav	*p = newWav(&fmt);
	if (!p) return (NULL);

	// dataチャンクの入力
	if (!isChunk(fp, "data")) return (NULL);
	p->size = readSize(fp);
	if (p->size == 0) return (NULL);
	p->data = (unsigned char *)malloc(p->size);
	if (!p->data) return (NULL);
	if (fread(p->data, 1, p->size, fp) < p->size) goto ERROR;

	// 独自拡張メンバの設定
	p->len = p->size/p->fmt.bs;
	p->time = (double)(p->len)/(double)(p->fmt.fs);
	p->byte = p->fmt.bs/p->fmt.ch;
	for (int c = 0; c < p->fmt.ch; c++) {
		p->val[c] = (double *)malloc(sizeof(double)*p->len);
		if (!p->val[c]) goto ERROR;
	}
	unsigned char	*d = p->data;
	for (unsigned int i = 0; i < p->len; i++)
	for (int c = 0; c < p->fmt.ch; c++) {
	//	p->val[c][i] = bin2val(d, p->byte);
		p->val[c][i] = dat2val(d, p->byte);
		d += p->byte;
	}
	return (p);
ERROR:
	freeWav(p);
	return (NULL);
}

Wav *pcmLoad(const char *fn)
{
	if (!fn) return (NULL);

	Wav	*p;
	if (strcmp(fn, "-") == 0) {
		p = pcmRead(stdin);
	} else {
		FILE	*fp = fopen(fn, "r");
		if (!fp) return (NULL);

		p = pcmRead(fp);
		fclose(fp);
	}
	return (p);
}

// PCMデータのWAVファイル出力
int pcmWrite(FILE *fp, const Wav *p)
{
	if (!fp) return (0);
	if (!p) return (0);
	if (p->size == 0) return (0);
	if (!p->data) return (0);

	// 標本の実数値配列→バイナリデータ列
	unsigned char	*d = p->data;
	for (unsigned int i = 0; i < p->len; i++)
	for (int c = 0; c < p->fmt.ch; c++) {
	//	unsigned int	bin = val2bin(p->val[c][i], p->byte);
	//	memcpy(d, &bin, p->byte);
		val2dat(d, p->val[c][i], p->byte);
		d += p->byte;
	}

	// RIFFヘッダ
	writeData(fp, 4, "RIFF");
	unsigned int	size = p->size + 36;
		// 36 = "WAVE"の4 + fmtチャンクの24 + "data"とlenの8
	writeData(fp, 4, &size);
	writeData(fp, 4, "WAVE");	// 4[B]

	// fmtチャンク
	writeData(fp, 4, "fmt ");
	writeData(fp, 4, &p->fmtSize);
	writeData(fp, 16, &p->fmt);

	// dataチャンク
	writeData(fp, 4, "data");
	writeData(fp, 4, &p->size);
	writeData(fp, p->size, p->data);
	return (1);
}

int pcmSave(const char *fn, const Wav *p)
{
	if (!fn) return (0);
	if (!p) return (0);

	if (strcmp(fn, "-") == 0) return (pcmWrite(stdout, p));

	FILE	*fp = fopen(fn, "w");
	if (!fp) return (0);
	int	r = pcmWrite(fp, p);
	fclose(fp);
	return (r);
}

// PCMデータ属性のファイル出力
int pcmInfo(FILE *fp, const Wav *p)
{
	if (!fp) return (0);
	if (!p) return (0);
	if (p->fmtSize != 16) return (0);	// not PCM
	if (p->fmt.type != 1) return (0);	// not linear PCM

	fprintf(fp, "チャネル数：	%d\n", p->fmt.ch);
	fprintf(fp, "標本化周波数：	%d [Hz]\n", p->fmt.fs);
	fprintf(fp, "量子化深度：	%d [bits]\n", p->fmt.bit);
	fprintf(fp, "データサイズ：	%d [bytes]\n", p->size);
	fprintf(fp, "標本数：	%d\n", p->len);
	fprintf(fp, "収録時間：	%f [s]\n", p->time);
	fprintf(fp, "\n");
	return (1);
}

// 音階名or周波数の文字列から周波数の数値への変換
double pcmMusicFreq(const char *s)
{
	if (!s) return (0.0);

	// 数字列→ 周波数
	if (isdigit(*s)) return (atof(s));

	// 音階
	char	c = toupper(*s);
	if ((c < 'A') || ('G' < c)) return (0.0);
	static int	scale[] = {0, 2, -9, -7, -5, -4, -2};	// A, B, ..., G の音階番号
	int	k = scale[c - 'A'];
	s++;

	// 半音上下
	switch (*s) {
	case '#': case '+': k++; s++; break;	// ♯
	case 'b': case '-': k--; s++; break;	// ♭
	default: break;
	}

	// オクターブ
	int	oct = 4;
	if (*s) oct = atoi(s);
	if ((oct < 0) || (9 < oct)) return (0.0);

	// 周波数
	return (F_A4*pow(2.0, (double)k/12.0 + (oct - 4)));
}

#ifdef __cplusplus
}
#endif

