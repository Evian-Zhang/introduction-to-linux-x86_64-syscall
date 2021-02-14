build : open-cloexec.o child.o
	cc -o open-cloexec open-cloexec.o
	cc -o child child.o
	echo -n "123456" > text.txt

open-cloexec.o : open-cloexec.c
	cc -c open-cloexec.c

child.o : child.c
	cc -c child.c

clean :
	rm *.o open-cloexec child text.txt

.PHONY: build clean