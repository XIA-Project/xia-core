include ../../../../../xia.mk

.PHONY: clean

DAEMONS_COMMON_PATH=../../../../common
DAEMONS_COMMON_INCLUDE=$(DAEMONS_COMMON_PATH)
DAEMONS_COMMON=$(DAEMONS_COMMON_PATH)/csclient.o $(DAEMONS_COMMON_PATH)/XIARouter.o

CFLAGS+=-g -I$(DAEMONS_COMMON_INCLUDE)
LDFLAGS+=$(LIBS) -lprotobuf -lpthread -lm
TARGETS= daemons_common removeAfterAdd

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

setCIDRoute.o: setCIDRoute.cpp
	$(CC) $(CFLAGS) -c setCIDRoute.cpp -std=c++11

setCIDRoute: setCIDRoute.o
	$(CC) -o setCIDRoute setCIDRoute.o csclient.o XIARouter.o $(LDFLAGS) -std=c++11

putChunk.o: putChunk.cpp
	$(CC) $(CFLAGS) -c putChunk.cpp -std=c++11

putChunk: putChunk.o
	$(CC) -o putChunk putChunk.o $(LDFLAGS) -std=c++11

requestChunk.o: requestChunk.cpp
	$(CC) $(CFLAGS) -c requestChunk.cpp -std=c++11

requestChunk: requestChunk.o
	$(CC) -o requestChunk requestChunk.o $(LDFLAGS) -std=c++11

removeAfterAdd.o: removeAfterAdd.cpp
	$(CC) $(CFLAGS) -c removeAfterAdd.cpp -std=c++11

removeAfterAdd: removeAfterAdd.o
	$(CC) -o removeAfterAdd removeAfterAdd.o csclient.o XIARouter.o $(LDFLAGS) -std=c++11

clean:
	rm -f testCIDSetRoute testAlarmHandler configureLocalCIDs setCIDRoute putChunk requestChunk removeAfterAdd
	rm -f *.o