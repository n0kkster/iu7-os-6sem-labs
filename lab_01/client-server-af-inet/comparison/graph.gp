set terminal pdfcairo size 16cm, 10cm enhanced
set output 'time_plot.pdf'

set xlabel "Номер результата"
set ylabel "Время (мкс)"

set title "График времени выполнения"

set grid

set style data lines

set autoscale x
set autoscale y

set xrange [*:*]
set yrange [*:*]
; set yrange [*:100] 

plot "mt.txt" using 0:1 title "Многопоточный сервер" with lines, \
	"mp.txt" using 0:1 title "Мультиплекс сервер" with lines \
