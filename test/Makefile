CXX=g++
CC=g++
LIBS=-lpthread -lasound
all: check createtest latency
createtest: createtest.o
check: check.o
latency: latency.o
	$(CXX) $< -o latency  $(LIBS)
clean:
	rm *.o
	rm check createtest latency
