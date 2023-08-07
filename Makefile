SOURCES = $(wildcard src/**/*.cpp src/*.cpp)
OBJS = $(SOURCES:.cpp=.o)

LLVM-CONFIG = llvm-config-14

FLAGS = -O3 -Iinclude -Wall -Wextra -Wno-redundant-move -Wno-unused-variable -Wno-reorder -Wno-switch -Wno-unused-parameter -Wno-non-pod-varargs

ifeq ($(DEBUG),true)
	FLAGS += -g
endif

CXXFLAGS = $(FLAGS) -fno-exceptions -c -std=c++14 $(shell $(LLVM-CONFIG) --cflags)
CFLAGS = -c

LDFLAGS = $(FLAGS) $(shell $(LLVM-CONFIG) --ldflags --libs --system-libs core)

build: compiler

compiler: main.o $(OBJS)
	mkdir -p bin
	$(CXX) main.o $(OBJS) $(LDFLAGS) -o bin/quart

jit: main-jit.o lib/panic.o $(OBJS)
	mkdir -p bin
	$(CXX) main-jit.o lib/panic.o $(OBJS) -rdynamic $(LDFLAGS) -lpthread -o bin/quart-jit

all: source jit compiler

source: $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -fr main.o main-jit.o $(OBJS) bin/quart bin/quart-jit
