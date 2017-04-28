.PHONY: proto stored-test store-test clean

CC 				= g++

MONGOD_CLFLAGS	= `pkg-config --cflags libmongoc-1.0 libbson-1.0` -LLIBDIR
MONGOD_LDFLAGS	= `pkg-config --libs libmongoc-1.0 libbson-1.0`

CFLAGS 			+= `pkg-config --cflags protobuf` $(MONGOD_CLFLAGS) -Wall -g
LDFLAGS 		+= `pkg-config --libs protobuf` $(MONGOD_LDFLAGS) -pthread -lprotobuf
SOURCES 		= store.cc wrapper.cc bcon-wrapper.cc helper.cc
DEPS			= proto/chunk.pb.cc proto/operation.pb.cc
OBJ 			= $(SOURCES:.cc=.o)
TARGET 			= stored

PROTO_DIR		= "./proto"

all: proto $(TARGET)

proto:
	$(MAKE) -C $(PROTO_DIR)

$(TARGET): $(OBJ)
	$(CC) -I$(PROTO_DIR) $(CFLAGS) $(DEPS) stored.cc $^ -o $@ $(LDFLAGS)
	ln -f $(TARGET) ../../bin/

%.o: %.cc
	$(CC) -I$(PROTO_DIR) $(CFLAGS) -c $< -o $@

test: proto $(OBJ) store-test.o stored-test.o stored-test-batch.o
	$(CC) -I$(PROTO_DIR) -g $(CFLAGS) $(SOURCES) $(DEPS) store-test.o -o store-test $(LDFLAGS)
	$(CC) -I$(PROTO_DIR) -g $(CFLAGS) $(SOURCES) $(DEPS) stored-test.o -o stored-test $(LDFLAGS)
	$(CC) -I$(PROTO_DIR) -g $(CFLAGS) $(SOURCES) $(DEPS) stored-test-batch.o -o stored-test-batch $(LDFLAGS)

clean:
	$(MAKE) clean -C $(PROTO_DIR)
	-rm -f $(TARGET) *.o store-test proto/*.o store-test stored-test stored-test-batch
