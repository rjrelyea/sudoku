NSS_CFLAGS=$(shell pkg-config --cflags nss)
NSS_LDFLAGS=$(shell pkg-config --libs nss)
SOFTOKEN_LDFLAGS=$(shell pkg-config --libs nss-softokn)
NSPR_LDFLAGS=$(shell pkg-config --libs nspr)
NSS_BUILD=/builds/nss-hg-2
#OBJDIR=Linux2.6_x86_64_glibc_PTH_64_DBG.OBJ/Linux_SINGLE_SHLIB
MPI_CFLAGS=-I$(NSS_BUILD)/nss/lib/freebl -L$(NSS_BUILD)/nss/lib/freebl/mpi

CMDS=sudoku


all: $(CMDS) $(BUILDS)

install: $(CMDS)
	cp $(CMDS) ~/bin

clean:
	rm -f $(CMDS) $(BUILDS)


sudoku: sudoku.c
	cc -o sudoku -g  $<
