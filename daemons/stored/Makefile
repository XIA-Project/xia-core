.PHONY: test clean

CC 				= g++

MONGOD_CLFLAGS	= `pkg-config --cflags libmongoc-1.0 libbson-1.0` -LLIBDIR
MONGOD_LDFLAGS	= `pkg-config --libs libmongoc-1.0 libbson-1.0`

CFLAGS	+= `pkg-config --cflags protobuf` $(MONGOD_CLFLAGS) -Wall -g
LDFLAGS	+= `pkg-config --libs protobuf` $(MONGOD_LDFLAGS) -pthread -lprotobuf
SOURCES = chunk.pb.cc operation.pb.cc store.cc wrapper.cc bcon-wrapper.cc helper.cc 
OBJ 	= $(SOURCES:.cc=.o)
PROTOS 	= chunk.proto operation.proto
TARGET 	= stored

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -I$(PROTO_DIR) $(CFLAGS) stored.cc $^ -o $@ $(LDFLAGS)
	ln -f $(TARGET) ../../bin/

%.o: %.cc
	$(CC) -I$(PROTO_DIR) $(CFLAGS) -c $< -o $@

%.pb.cc %.pb.h: %.proto
	protoc --cpp_out=. $(PROTOS)

test: $(OBJ) store-test.o stored-test.o stored-test-batch.o
	$(CC) -I$(PROTO_DIR) -g $(CFLAGS) $(SOURCES) $(DEPS) store-test.o -o store-test $(LDFLAGS)
	$(CC) -I$(PROTO_DIR) -g $(CFLAGS) $(SOURCES) $(DEPS) stored-test.o -o stored-test $(LDFLAGS)
	$(CC) -I$(PROTO_DIR) -g $(CFLAGS) $(SOURCES) $(DEPS) stored-test-batch.o -o stored-test-batch $(LDFLAGS)

clean:
	-rm -f $(TARGET) *.o store-test store-test stored-test stored-test-batch *.pb.cc *.pb.h
