all: WTF.c helperfunctions.o WTFserver.c
	gcc -fsanitize=address -o WTF WTF.c helperfunctions.o -lssl -lcrypto -lm
	gcc -fsanitize=address -o WTFserver WTFserver.c helperfunctions.o -lpthread -DMUTEX -lm -lssl -lcrypto

helperfunctions.o: helperfunctions.c
	gcc -c helperfunctions.c -lm -lssl -lcrypto

test: WTFtest.c wtf.o
	gcc -o WTFtest WTFtest.c wtf.o

wtf.o: wtf.c
	gcc -c wtf.c

clean:
	rm -f WTF
	rm -f WTFserver
	rm -f helperfunctions.o
	rm -rf .server_directory
	rm -f .configure
	rm -f wtf.o
	rm -f WTFtest
