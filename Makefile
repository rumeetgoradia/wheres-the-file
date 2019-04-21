all: WTF.c
	gcc -fsanitize=address -o WTF WTF.c -lssl -lcrypto

clean:
	rm -f WTF
	rm -f .configure
	rm -f test/.Manifest
