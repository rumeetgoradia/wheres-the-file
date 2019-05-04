all: WTF.c helperfunctions.o WTFserver.c
	gcc -fsanitize=address -o WTF WTF.c helperfunctions.o -lssl -lcrypto -lm
	gcc -fsanitize=address -o WTFserver WTFserver.c helperfunctions.o -lpthread -DMUTEX -lm -lssl -lcrypto

helperfunctions.o: helperfunctions.c
	gcc -c helperfunctions.c -lm -lssl -lcrypto

clean:
	rm -f WTF
	rm -f WTFserver
	rm -f testclient
	rm -f test/.Manifest
	rm -f helperfunctions.o	

