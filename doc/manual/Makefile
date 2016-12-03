all: manual

# MANUALS=src/manuals
LATEXOPTS=

# export TEXINPUTS = .:$(HOME)/$(MANUALS):


manual: OpenTTPManual.tex
	pdflatex $(LATEXOPTS) OpenTTPManual.tex && pdflatex OpenTTPManual.tex

OpenTTPManual.tex: srcs/lcdmonitor.tex