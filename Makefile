EXE = webproxy

CXX = gcc
CXXFLAGS = -g -pthread

all: $(EXE)

web_proxy.o: web_proxy.c
	$(CXX) $(CXXFLAGS) -c -o web_proxy.o web_proxy.c

webproxy: web_proxy.o
	$(CXX) -Wall -pthread -o $(EXE) web_proxy.o

run: webproxy
	./webproxy 10001&

clean:
	rm -f $(EXE) *.o

