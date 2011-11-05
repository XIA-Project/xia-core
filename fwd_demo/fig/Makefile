all: dag_fb0.png dag_fb1.png dag_fb2.png dag_fb3.png hdr_fb0.png hdr_fb1.png hdr_fb2.png hdr_fb3.png

%.pdf: %.tex
	pdflatex $<
	rm *.log *.aux

%.png: %.pdf
	pdfcrop $< $<
	convert $< $@

%.png: %.svg
	inkscape -z --export-png=$@ --export-area-drawing --export-dpi=36 $<

clean:
	rm -f *.png *.pdf

