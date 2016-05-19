# Rui Meireles
# rui@cmu.edu

TOOLCHAINDIR=/opt/buildroot-2013.11/output/host/usr/

CC=mips-linux-gcc

CFLAGS=-Wall -march=24kc -D_GLIBCXX_USE_NANOSLEEP 

# to debug invoke with "make DEBUG=1"
ifdef DEBUG
	CFLAGS+=-ggdb -DDEBUG
else
	CFLAGS+=-O2 -fPIC -fomit-frame-pointer
endif


LDFLAGS=-L $(TOOLCHAINDIR)/lib -lwave -lpthread

export PATH:=$(TOOLCHAINDIR)/bin/:/$(PATH) # adds toolchain dir to path
OBJDIR= ./

INCS= -I $(TOOLCHAINDIR)/include/
INCS+= -I $(OBJDIR)

ALL=tcpserver

all: $(ALL)

%.o: %.cc
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

tcpserver: tcpserver.o
	$(CC) $(CFLAGS) -g $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o
	rm -f $(ALL)
