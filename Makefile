SOURCES = $(wildcard src/*.cpp src/types/*.cpp)
HEADERS = $(wildcard src/*.h src/types/*.h)
OBJS = $(SOURCES:.cpp=.o)

FLAGS = -O3
LLVM-CONFIG = llvm-config-14

ifeq ($(DEBUG),true)
	FLAGS += -g
endif

CXXFLAGS = $(FLAGS) -Iinclude -Wall -Wextra -Wno-reorder -Wno-switch -Wno-unused-parameter -Wno-non-pod-varargs -c -std=c++14
CXXFLAGS += $(shell $(LLVM-CONFIG) --cflags)

LDFLAGS = $(FLAGS) -Wno-non-pod-varargs
LDFLAGS += $(shell $(LLVM-CONFIG) --ldflags --libs --system-libs core)

build: main.o $(OBJS)
	$(CXX) main.o $(OBJS) $(LDFLAGS) -o proton

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run: build
	./main

clean:
ifeq ($(OS),Windows_NT)
	del /f /q /s *.o
else
	rm -fr main.o src/*.o src/types/*.o
endif