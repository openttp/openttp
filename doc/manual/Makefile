all: manual-ps manual-pdf

# MANUALS=src/manuals
LATEXOPTS=

# export TEXINPUTS = .:$(HOME)/$(MANUALS):


manual-ps: OpenTTPManual.tex
	latex $(LATEXOPTS) OpenTTPManual.tex && latex OpenTTPManual.tex && dvips -o OpenTTPManual.ps OpenTTPManual.dvi

manual-pdf: manual-ps
	ps2pdf OpenTTPManual.ps

OpenTTPManual.tex: srcs/lcdmonitor.tex