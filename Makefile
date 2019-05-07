all: WTF.c helperfunctions.o WTFserver.c
	gcc -fsanitize=address -o WTF WTF.c helperfunctions.o -lssl -lcrypto -lm
	gcc -fsanitize=address -o WTFserver WTFserver.c helperfunctions.o -lpthread -DMUTEX -lm -lssl -lcrypto

helperfunctions.o: helperfunctions.c
	gcc -c helperfunctions.c -lm -lssl -lcrypto


test: WTFtest.c
	gcc WTFtest.c -o WTFtest

clean:
	rm -f WTF
	rm -f WTFserver
	rm -f helperfunctiions.o	
	


