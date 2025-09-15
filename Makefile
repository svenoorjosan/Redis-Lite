CXX := g++
CXXFLAGS := -O2 -std=c++20 -Wall -Wextra -pedantic

SRC := cpp/server.cpp cpp/resp.cpp cpp/commands.cpp cpp/aof.cpp cpp/metrics.cpp
OBJ := $(SRC:.cpp=.o)

all: redis-lite

redis-lite: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

clean:
	rm -f $(OBJ) redis-lite
