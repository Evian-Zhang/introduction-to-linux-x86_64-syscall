build : write-pwrite.o
	cc -o write-pwrite write-pwrite.o
	echo -n "123456" > text.txt

write-pwrite.o : write-pwrite.c
	cc -c write-pwrite.c

clean :
	rm *.o write-pwrite text.txt

.PHONY: build clean