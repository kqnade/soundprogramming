DIR = pcm
LIB = libpcm.so
APP = thru rate sin wave synth seq mix ft

all:	$(LIB)	$(APP)

libpcm.so:	pcm.c	pcm.h
	cc $< -std=c99 -I. -lm -fpic -shared -o $@

ft:	ft.c	$(LIB)
	cc $< -std=c99 -I. -L. -lpcm -lm -o $@

thru:	thru.c	$(LIB)
	cc $< -std=c99 -I. -L. -lpcm -o $@

rate:	rate.c	$(LIB)
	cc $< -std=c99 -I. -L. -lpcm -lm -o $@

sin:	sin.c	$(LIB)
	cc $< -std=c99 -I. -L. -lpcm -lm -o $@

wave:	wave.c	$(LIB)
	cc $< -std=c99 -I. -L. -lpcm -lm -o $@

synth:	synth.c	$(LIB)
	cc $< -std=c99 -I. -L. -lpcm -lm -o $@

seq:	seq.c	$(LIB)
	cc $< -std=c99 -I. -L. -lpcm -o $@

mix:	mix.c	$(LIB)
	cc $< -std=c99 -I. -L. -lpcm -o $@

clean:
	-rm $(LIB) $(APP)

dist:	clean
	( cd ..; tar zcvf $(DIR).tgz $(DIR) )

