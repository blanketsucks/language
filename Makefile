SOURCES = $(wildcard src/**/*.cpp src/*.cpp)

OBJS = $(SOURCES:.cpp=.o)

FLAGS = -O3 -Iinclude -Wall -Wextra -Wno-reorder -Wno-switch -Wno-unused-parameter -Wno-non-pod-varargs
LLVM-CONFIG = llvm-config-14

ifeq ($(DEBUG),true)
	FLAGS += -g
endif

CXXFLAGS = $(FLAGS) -fno-exceptions -c -std=c++14 $(shell $(LLVM-CONFIG) --cflags)
LDFLAGS = $(FLAGS) $(shell $(LLVM-CONFIG) --ldflags --libs --system-libs core)

build: main.o $(OBJS)
	$(CXX) main.o $(OBJS) $(LDFLAGS) -o proton

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -fr main.o $(OBJS) proton