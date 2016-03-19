all: Manual

# MANUALS=src/manuals
LATEXOPTS=

# export TEXINPUTS = .:$(HOME)/$(MANUALS):


Manual: DevDocs.tex
	latex $(LATEXOPTS) DevDocs.tex && latex DevDocs.tex
	dvips -o DevDocs.ps DevDocs.dvi

DevDocs.tex: srcs/mktimetx.tex