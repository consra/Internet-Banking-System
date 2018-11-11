CC=g++
LIBSOCKET=-lnsl -Wall -g -std=c++11
CCFLAGS=-Wall -g -std=c++11
SRV=server
SEL_SRV=server
CLT=client

all: $(SEL_SRV) $(CLT)

$(SEL_SRV):$(SEL_SRV).cpp
	$(CC) -o $(SEL_SRV) $(LIBSOCKET) $(SEL_SRV).cpp

$(CLT):	$(CLT).cpp
	$(CC) -o $(CLT) $(LIBSOCKET) $(CLT).cpp
run_server: $(SEL_SRV)
	./server 2500 users_file_data
run_client: $(CLT)
	./client "127.0.0.1" 2500
clean:
	rm -f *.o *~
	rm -f $(SEL_SRV) $(CLT)
