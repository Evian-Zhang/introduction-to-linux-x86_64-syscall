build : lseek-hole.o
	cc -o lseek-hole lseek-hole.o
	echo -n "123456" > text.txt

lseek-hole.o : lseek-hole.c
	cc -c lseek-hole.c

clean :
	rm *.o lseek-hole text.txt

.PHONY: build clean