all: WTF.c clientfunctions.o
	gcc -fsanitize=address -o WTF WTF.c clientfunctions.o -lssl -lcrypto

clientfunctions.o: clientfunctions.c
	gcc -c clientfunctions.c

clean:
	rm -f WTF
	rm -f .configure
	rm -f test/.Manifest
	rm -f clientfunctions.o
