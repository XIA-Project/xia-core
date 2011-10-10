.PHONY: clean

CFLAGS=-I../API

xc: Client.c 
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a
pc: pingClient.c
	gcc $(CFLAGS) -o $@ $< ../API/libXsocket.a

clean:
	rm -f xs ps pc xc
