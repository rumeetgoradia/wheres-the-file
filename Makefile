all: WTF.c
	gcc -fsanitize=address -o WTF WTF.c

clean:
	rm -f WTF
	rm -f .configure
