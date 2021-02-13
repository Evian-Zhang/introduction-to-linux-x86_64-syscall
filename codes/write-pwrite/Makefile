build : write-pwrite.o
	cc -o write-pwrite write-pwrite.o
	echo -n "123456" > text.txt

clean :
	rm *.o write-pwrite text.txt

.PHONY: build clean