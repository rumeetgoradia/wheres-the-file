all: WTF.c clientfunctions.o WTFserver.c
	gcc -fsanitize=address -o WTF WTF.c clientfunctions.o -lssl -lcrypto -lm
	gcc -fsanitize=address -o WTFserver WTFserver.c -lpthread -DMUTEX

test: testclient.c WTFserver.c
	gcc -fsanitize=address -o WTFserver WTFserver.c -lpthread -DMUTEX
	gcc -fsanitize=address -o testclient testclient.c

clientfunctions.o: clientfunctions.c
	gcc -c clientfunctions.c

clean:
	rm -f WTF
	rm -f WTFserver
	rm -f testclient
	rm -f .configure
	rm -f test/.Manifest
	rm -f clientfunctions.o
