all: crsd crc


crsd: 
	g++ crsd.c -o crsd
	
crc: interface.h
	g++ -pthread crc.c -o crc

clean:
	rm -f *.o crsd crc
