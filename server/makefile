all:server

server:server.cpp
	g++ -o server server.cpp -pthread -ljsoncpp -lmysqlclient -lcrypto
clean:
	rm -f server
