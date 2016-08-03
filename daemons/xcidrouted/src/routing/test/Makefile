include ../../../../../xia.mk

.PHONY: clean

DAEMONS_COMMON_PATH=../../../../common
DAEMONS_COMMON_INCLUDE=$(DAEMONS_COMMON_PATH)
DAEMONS_COMMON=$(DAEMONS_COMMON_PATH)/csclient.o $(DAEMONS_COMMON_PATH)/XIARouter.o

CFLAGS+=-g -I$(DAEMONS_COMMON_INCLUDE)
LDFLAGS+=$(LIBS) -lprotobuf -lpthread -lm
TARGETS= daemons_common configureLocalCIDs

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

configureLocalCIDs.o: configureLocalCIDs.cpp
	$(CC) $(CFLAGS) -c configureLocalCIDs.cpp -std=c++11

configureLocalCIDs: configureLocalCIDs.o
	$(CC) -o configureLocalCIDs configureLocalCIDs.o csclient.o XIARouter.o $(LDFLAGS) -std=c++11

clean:
	rm -f testCIDSetRoute testAlarmHandler configureLocalCIDs
	rm -f *.o