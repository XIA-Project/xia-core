.PHONY: clean
CC=g++
CFLAGS1=-Wall -Wextra
CFLAGS2=-I../API
LDFLAGS =  -lprotobuf -lpthread
SOURCES1=routetest.cc csclient.cc XIARouter.cc
SOURCES2=xroute.cc csclient.cc XIARouter.cc ../API/libXsocket.a

all: routetest xroute xroute_r3

routetest: $(SOURCES1)
	$(CC) -o $@ $(CFLAGS1) $(SOURCES1)

xroute: $(SOURCES2)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES2) $(LDFLAGS)

xroute_r3: $(SOURCES2)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES2) $(LDFLAGS)

clean:
	rm xroute routetest xroute_r0 
	
