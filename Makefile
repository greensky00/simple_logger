LDFLAGS = -pthread

CXXFLAGS = \
	-g -D_GNU_SOURCE \
	-I. -I./src -I./examples -I./tests

CXXFLAGS += -Wall -std=c++11
#CXXFLAGS += -O3

BASIC_TEST = \
	src/logger.o \
	tests/logger_test.o \

BASIC_EXAMPLE = \
	src/logger.o \
	example/basic_example.o \

CRASH_EXAMPLE = \
	src/logger.o \
	example/crash_example.o \

PROGRAMS = \
	tests/logger_test \
	example/basic_example \
	example/crash_example \

all: $(PROGRAMS)

tests/logger_test: $(BASIC_TEST)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

example/basic_example: $(BASIC_EXAMPLE)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

example/crash_example: $(CRASH_EXAMPLE)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test:
	tests/basic_test

clean:
	rm -rf $(PROGRAMS) ./*.o ./*.so ./*/*.o ./*/*.so
