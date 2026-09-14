#!/usr/bin/gnuplot -c
# plot discrete spectra
# Usage: ds.plot "DATA.txt"
if (ARGC < 1) quit
DATA = ARG1
set title "discrete spectra : ".DATA
set xlabel "frequency {/:Italic f_k}" font "Times,24"
set ylabel "amplitude  |{/:Italic X_k}|" font "Times,24"
set lmargin 10
set bmargin 5
unset key
plot DATA with impulses lw 1, DATA with dots lw 3
pause -1
