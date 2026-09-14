#ifndef AUDIO_ARGS_H
#define AUDIO_ARGS_H
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int integer_arg(const char *text, int minimum, int maximum)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end || value < minimum || value > maximum) {
        fprintf(stderr, "整数引数が範囲外です: %s (%d..%d)\n", text, minimum, maximum);
        exit(1);
    }
    return (int)value;
}
#endif
