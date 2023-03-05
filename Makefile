SOURCES = $(wildcard src/**/*.cpp src/*.cpp)
OBJS = $(SOURCES:.cpp=.o)

FLAGS = -O3 -Iinclude -Wall -Wextra -Wno-redundant-move -Wno-unused-variable -Wno-reorder -Wno-switch -Wno-unused-parameter -Wno-non-pod-varargs
LLVM-CONFIG = llvm-config-14

ifeq ($(DEBUG),true)
	FLAGS += -g
endif

CXXFLAGS = $(FLAGS) -fno-exceptions -c -std=c++14 $(shell $(LLVM-CONFIG) --cflags)
CFLAGS = -c

LDFLAGS = $(FLAGS) $(shell $(LLVM-CONFIG) --ldflags --libs --system-libs core)

build: compiler

compiler: main.o $(OBJS)
	$(CXX) main.o $(OBJS) $(LDFLAGS) -o quart

jit: main-jit.o lib/panic.o $(OBJS)
	$(CXX) main-jit.o lib/panic.o $(OBJS) $(LDFLAGS) -lpthread -o quart-jit

all: source jit compiler

source: $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -fr main.o main-jit.o $(OBJS) quart quart-jit
