LDFAGS=-L /usr/lib64/mysql
FLAG=-lmysqlclient -lpthread -ljsoncpp -lcrypto -std=c++11
BIN=../bin/ais_svr
CC=g++

.PHONY:$(BIN)
$(BIN):ais_system.cpp
			$(CC) $^ -o $@ $(LDFAGS) $(FLAG)

.PHONY:clean
clean:
			rm -rf $(BIN)
