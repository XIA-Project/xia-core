.PHONY: proto stored-test store-test clean

CC 				= g++

MONGOD_CLFLAGS	= `pkg-config --cflags libmongoc-1.0` -LLIBDIR
MONGOD_LDFLAGS	= `pkg-config --libs libmongoc-1.0`

CFLAGS 			+= `pkg-config --cflags protobuf` $(MONGOD_CLFLAGS) -Wall -g
LDFLAGS 		+= `pkg-config --libs protobuf` $(MONGOD_LDFLAGS) -pthread -lprotobuf
SOURCES 		= stored.cc store.cc wrapper.cc bcon-wrapper.cc helper.cc proto/chunk.pb.cc proto/operation.pb.cc
OBJ 			= $(SOURCES:.cc=.o)
TARGET 			= stored

PROTO_DIR		= "./proto/"

TEST_SOURCES    = proto/*.cc wrapper.cc helper.cc
TEST_CFLAGS 	= -g
TEST_LDFLAGS 	= -pthread

all: proto $(TARGET) stored-test

proto:
	$(MAKE) -C $(PROTO_DIR)

$(TARGET): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)
	ln -f $(TARGET) ../../bin/

%.o: %.cc
	$(CC) $(CFLAGS) -c $< -o $@

stored-test: proto
	$(CC) $(TEST_CFLAGS) $(CFLAGS) $(TEST_SOURCES) mstore.cc -o mstore $(TEST_LDFLAGS) $(LDFLAGS)
	$(CC) $(TEST_CFLAGS) $(CFLAGS) $(TEST_SOURCES) mstore-test.cc -o mstore-test $(TEST_LDFLAGS) $(LDFLAGS)

store-test: proto
	$(CC) $(CFLAGS) wrapper.cc bcon-wrapper.cc store.cc store-test.cc proto/*.cc helper.cc -o store-test $(LDFLAGS)

test: proto
	$(CC) $(TEST_CFLAGS) $(CFLAGS) $(TEST_SOURCES) test.cc -o test $(TEST_LDFLAGS) $(LDFLAGS)

clean:
	$(MAKE) clean -C $(PROTO_DIR)
	-rm -f $(TARGET) *.o store-test proto/*.o mstore mstore-test
