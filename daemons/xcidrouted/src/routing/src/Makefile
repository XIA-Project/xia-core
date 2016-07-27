include ../../../../../xia.mk

.PHONY: clean

DAEMONS_COMMON_PATH=../../../../common
DAEMONS_COMMON_INCLUDE=$(DAEMONS_COMMON_PATH)
DAEMONS_COMMON=$(DAEMONS_COMMON_PATH)/csclient.o $(DAEMONS_COMMON_PATH)/XIARouter.o

CFLAGS+=-g -I$(DAEMONS_COMMON_INCLUDE)
LDFLAGS+=$(LIBS) -lprotobuf -lpthread -lm
TARGETS= daemons_common xcidrouted

all: $(TARGETS)

xcidrouted.o: xcidrouted.cc xcidrouted.h
	$(CC) $(CFLAGS) -c xcidrouted.cc -std=c++11

daemons_common: $(DAEMONS_COMMON)
	mv $(DAEMONS_COMMON_PATH)/*.o .

xcidrouted: xcidrouted.o
	g++ -o xcidrouted xcidrouted.o csclient.o XIARouter.o $(LDFLAGS) -std=c++11

clean:
	rm -f xcidrouted
	rm -f *.o