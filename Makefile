CC=/usr/local/opt/llvm/bin/clang++
LDFLAGS=-L/usr/local/opt/llvm/lib -lc++ -lc++experimental
CPPFLAGS=-I/usr/local/opt/llvm/include/ -I/usr/local/opt/llvm/include/c++/v1/
CPPFLAGS_COMMON=-std=c++17 -stdlib=libc++ -Wall -Wextra -Wshadow -pedantic
BOOST_ASIO_INCLUDE=-I/Users/jvillasante/Hacking/software/asio-1.12.1/include -DASIO_STANDALONE
CFLAGS=$(LDFLAGS) $(CPPFLAGS) $(CPPFLAGS_COMMON) $(BOOST_ASIO_INCLUDE) -DNDEBUG -O3
CFLAGS_DEBUG=$(LDFLAGS) $(CPPFLAGS) $(CPPFLAGS_COMMON) $(BOOST_ASIO_INCLUDE) -g -DDEBUG -O0
SRC=src
BIN=bin
RM=rm -rf
CP=cp -rf

# The Cleaner
clean:
	@$(RM) $(BIN)/*

all: clean
	$(CC) $(CFLAGS) -o $(BIN)/client $(SRC)/chat_client.cpp
	$(CC) $(CFLAGS) -o $(BIN)/server $(SRC)/chat_server.cpp
