CXX = g++ --std=c++20
CXXFLAGS = -O3 -flto -march=native -g

TARGET = engine-perf

# source files
SRCS = test/perf-test.cpp \
	core/engine-main.cpp \
	core/attacks.cpp \
	core/evaluate.cpp \
	core/make-move.cpp \
	core/search.cpp \
	core/transposition-table.cpp

# object files
OBJS = $(SRCS:.cpp=.o)

# default target
build: $(TARGET)

# link
$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS)

# compile
%.o: %.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS)


clean:
	rm $(OBJS) $(TARGET)
#	rm $(SRCS:.cpp=.gcno) $(SRCS:.cpp=.gcda)

# phony targets
.PHONY: build clean
