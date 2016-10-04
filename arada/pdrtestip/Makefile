# Rui Meireles
# rui@cmu.edu

TOOLCHAINDIR=/opt/buildroot-2013.11/output/host/usr/

CPP=mips-linux-g++

CPPFLAGS=-Wall -march=24kc -std=c++11 -D_GLIBCXX_USE_NANOSLEEP 

# to debug invoke with "make DEBUG=1"
ifdef DEBUG
	CPPFLAGS+=-ggdb -DDEBUG
else
	CPPFLAGS+=-O2 -fPIC -fomit-frame-pointer
endif


LDFLAGS=-L $(TOOLCHAINDIR)/lib

export PATH:=$(TOOLCHAINDIR)/bin/:/$(PATH) # adds toolchain dir to path
OBJDIR= ./

INCS= -I $(TOOLCHAINDIR)/include/
INCS+= -I $(OBJDIR)

ALL=pdrtestipserver pdrtestipclient

all: $(ALL)

%.o: %.cc
	$(CPP) $(CPPFLAGS) $(INCS) -c $< -o $@

pdrtestipserver: pdrtestipserver.o
	$(CPP) $(CPPFLAGS) $(LDFLAGS) $^ -o $@

pdrtestipclient: pdrtestipclient.o
	$(CPP) $(CPPFLAGS) $(LDFLAGS) $^ -o $@

clean:
	/bin/rm -f *.o
	/bin/rm -f $(ALL)
