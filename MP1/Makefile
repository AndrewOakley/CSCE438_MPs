all: server client


server: 
	g++ crsd.c -o server
	
client: interface.h
	g++ -pthread crc.c -o client

clean:
	rm -f *.o server client