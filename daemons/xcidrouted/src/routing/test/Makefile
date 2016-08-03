include ../../../../../xia.mk

.PHONY: clean

DAEMONS_COMMON_PATH=../../../../common
DAEMONS_COMMON_INCLUDE=$(DAEMONS_COMMON_PATH)
DAEMONS_COMMON=$(DAEMONS_COMMON_PATH)/csclient.o $(DAEMONS_COMMON_PATH)/XIARouter.o

CFLAGS+=-g -I$(DAEMONS_COMMON_INCLUDE)
LDFLAGS+=$(LIBS) -lprotobuf -lpthread -lm
TARGETS= testAlarmHandler

all: $(TARGETS)

daemons_common: $(DAEMONS_COMMON)
	mv $(DAEMONS_COMMON_PATH)/*.o .

testCIDSetRoute.o: testCIDSetRoute.cpp
	$(CC) $(CFLAGS) -c testCIDSetRoute.cpp -std=c++11

testCIDSetRoute: testCIDSetRoute.o
	$(CC) -o testCIDSetRoute testCIDSetRoute.o csclient.o XIARouter.o $(LDFLAGS) -std=c++11

testAlarmHandler.o: testAlarmHandler.cpp
	$(CC) $(CFLAGS) -c testAlarmHandler.cpp -std=c++11

testAlarmHandler: testAlarmHandler.o
	$(CC) -o testAlarmHandler testAlarmHandler.o $(LDFLAGS) -std=c++11

clean:
	rm -f testCIDSetRoute testAlarmHandler
	rm -f *.o