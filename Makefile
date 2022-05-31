SOURCES = $(wildcard src/*.cpp)
HEADERS = $(wildcard src/*.h)
OBJS = $(SOURCES:.cpp=.o)

CC = g++

CFLAGS = -O3 -g `llvm-config-14 --cxxflags` -c
LDFLAGS = -O3 -g `llvm-config-14 --ldflags --libs --system-libs`

build: main.o $(OBJS)
	$(CC) main.o $(OBJS) $(LDFLAGS) -o main

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

run: build
	./main

clean:
	rm main.o src/*.o