#!/usr/bin/gnuplot -c
# plot discrete waveform
# Usage: dw.plot "DATA.txt"
if (ARGC < 1) quit
DATA = ARG1
set title "discrete waveform : ".DATA
set xlabel "time {/:Italic t_n}" font "Times,24"
set ylabel "instantaneous value  {/:Italic x_n}" font "Times,24"
set lmargin 10
set bmargin 5
unset key
plot DATA with impulses lw 1, DATA with dots lw 3
pause -1
