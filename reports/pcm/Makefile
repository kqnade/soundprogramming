CC ?= cc
CPPFLAGS += -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -I.
CFLAGS ?= -O2 -std=c99 -Wall -Wextra
BUILD ?= build
APP = thru rate sin sin2 wave synth seq mix dft ft iir ip fft fir
PROGRAMS = $(addprefix $(BUILD)/,$(APP))
HEADERS = $(wildcard *.h)

.PHONY: all clean
all: $(PROGRAMS)

$(BUILD):
	mkdir -p $@

$(BUILD)/libpcm.so: pcm.c pcm.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared pcm.c -lm -o $@

$(PROGRAMS): $(BUILD)/%: %.c $(HEADERS) $(BUILD)/libpcm.so
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -L$(BUILD) -lpcm -lm -Wl,-rpath,'$$ORIGIN' -o $@

clean:
	rm -f $(PROGRAMS) $(BUILD)/libpcm.so
