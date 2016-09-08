include ../../../../../xia.mk

.PHONY: clean

DAEMONS_COMMON_PATH=../../../../common
DAEMONS_COMMON_INCLUDE=$(DAEMONS_COMMON_PATH)
DAEMONS_COMMON=$(DAEMONS_COMMON_PATH)/csclient.o $(DAEMONS_COMMON_PATH)/XIARouter.o

LOG_PATH=../../log
LOG=$(LOG_PATH)/logger.o

CFLAGS+=-g -I$(DAEMONS_COMMON_INCLUDE)
LDFLAGS+=$(LIBS) -lprotobuf -lpthread -lm
TARGETS= daemons_common log xcidrouted xcidrouted_ls xcidrouted_dv

all: $(TARGETS)

log: $(LOG)
	mv $(LOG_PATH)/*.o .

daemons_common: $(DAEMONS_COMMON)
	mv $(DAEMONS_COMMON_PATH)/*.o .

xcidrouted_ls.o: xcidrouted_ls.cc xcidrouted_ls.hh
	$(CC) $(CFLAGS) -c xcidrouted_ls.cc -std=c++11

xcidrouted_ls: xcidrouted_ls.o
	g++ -o xcidrouted_ls xcidrouted_ls.o logger.o csclient.o XIARouter.o $(LDFLAGS) -std=c++11

xcidrouted_dv.o: xcidrouted_dv.cc xcidrouted_dv.hh
	$(CC) $(CFLAGS) -c xcidrouted_dv.cc -std=c++11

xcidrouted_dv: xcidrouted_dv.o
	g++ -o xcidrouted_dv xcidrouted_dv.o logger.o csclient.o XIARouter.o $(LDFLAGS) -std=c++11

xcidrouted.o: xcidrouted.cc xcidrouted.h
	$(CC) $(CFLAGS) -c xcidrouted.cc -std=c++11

xcidrouted: xcidrouted.o
	g++ -o xcidrouted xcidrouted.o logger.o csclient.o XIARouter.o $(LDFLAGS) -std=c++11

clean:
	rm -f xcidrouted xcidrouted_dv xcidrouted_ls
	rm -f *.o