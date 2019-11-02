DEPFILE	 = .depends
DEPTOKEN = '\# MAKEDEPENDS'
DEPFLAGS = -Y -f $(DEPFILE) -s $(DEPTOKEN) 
CSRCS    = $(wildcard *.cc) 
COBJS    = $(CSRCS:.cc=.o)
LIBD = -L./minisat/core
LIBS =
CXX?=g++

ifdef PROF 
CFLAGS+=-pg
LNFLAGS+=-pg
endif

ifdef DBG
CFLAGS+=-O0
CFLAGS+=-ggdb
CFLAGS+=-DDBG
MSAT=libd
else
CFLAGS+=-DNDEBUG
CFLAGS+=-O3
MSAT=libr
endif

CFLAGS += -Wall
CFLAGS+=-D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -Wno-parentheses -Wno-deprecated -D _MSC_VER
CFLAGS+=-D _MSC_VER # this is just for compilation of Options in minisat, which are not used anyhow
CFLAGS+=-I./minisat/
CFLAGS+=-std=c++0x
LIBS+=-lz -lminisat

ifdef STATIC
CFLAGS+=-static
LNFLAGS+=-static
endif

ifdef EXPERT # allow using giving options, without options the solver's fairly dumb
CFLAGS+=-DEXPERT
endif

.PHONY: m

all:  rareqs 

rareqs: m $(COBJS)
	@echo Linking: $@
	$(CXX) -o $@ $(COBJS) $(LNFLAGS) $(LIBD) $(LIBS)

m:
	export MROOT=`pwd`/minisat ; cd ./minisat/core; make CXX=$(CXX) LIB=minisat $(MSAT)

depend:
	rm -f $(DEPFILE)
	@echo $(DEPTOKEN) > $(DEPFILE)
	makedepend $(DEPFLAGS) -- $(CFLAGS) -- $(CSRCS)

## Build rule
%.o:	%.cc
	@echo Compiling: $@
	@$(CXX) $(CFLAGS) -c -o $@ $<

## Clean rule
clean:
	@rm -f rareqs rareqs.exe $(COBJS)
	@export MROOT=`pwd`/minisat ; cd ./minisat/core; make CXX=$(CXX) clean
	@rm -f ./minisat/core/libminisat*
	@rm -f $(DEPFILE)

### Makefile ends here
sinclude $(DEPFILE)
