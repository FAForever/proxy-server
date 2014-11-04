CXX=g++
CXXFLAGS=-O3 -mx32 -g

all: proxyserver testclient

clean:
	rm proxyserver testclient

proxyserver: proxyserver.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

testclient: testclient.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<
