// WAVファイル生成アプリ（sin.cの拡張版）
// 量子化深度・チャネル数・標本化周波数を引数で指定可能
//
// コンパイル：$ cc sin2.c -std=c99 -lpcm -lm -o sin2
// 使い方：$ ./sin2 [周波数 [量子化深度 [チャネル数 [標本化周波数]]]]
// 例：
//   $ ./sin2 440 8  1 48000 > out_8bit_mono_48k.wav
//   $ ./sin2 440 16 1 48000 > out_16bit_mono_48k.wav
//   $ ./sin2 440 24 1 48000 > out_24bit_mono_48k.wav
//   $ ./sin2 440 32 1 48000 > out_32bit_mono_48k.wav
//   $ ./sin2 440 16 2 48000 > out_16bit_stereo_48k.wav
//   $ ./sin2 440 16 1  8000 > out_16bit_mono_8k.wav
//   $ ./sin2 440 16 1 44100 > out_16bit_mono_44k.wav

#include <stdio.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#define __USE_XOPEN
#include <math.h>
#include "pcm.h"

#define debug(...) fprintf(stderr, __VA_ARGS__)

int main(int argc, char *argv[])
{
    double  f   = 440.0;    // 信号周波数 [Hz]
    int     bit = 16;       // 量子化深度
    int     ch  = 1;        // チャネル数
    int     fs  = 48000;    // 標本化周波数 [Hz]
    double  d   = 1.0;      // 音長 [s]

    if (argc > 1) f   = pcmMusicFreq(argv[1]);
    if (argc > 2) bit = atoi(argv[2]);
    if (argc > 3) ch  = atoi(argv[3]);
    if (argc > 4) fs  = atoi(argv[4]);

    double  w   = 2.0 * M_PI * f;
    int     len = (int)(d * fs);

    Wav *p = pcmInit(bit, ch, fs, len);
    if (p == NULL) return (1);

    debug("bit=%d  ch=%d  fs=%d  len=%d  f=%.1f Hz\n", bit, ch, fs, len, f);
    pcmInfo(stderr, p);

    for (int i = 0; i < len; i++) {
        double t = (double)i / (double)fs;
        double v = sin(w * t);
        for (int c = 0; c < ch; c++)
            p->val[c][i] = v;
    }

    pcmWrite(stdout, p);
    pcmFin(p);
    return (0);
}
