.PHONY:cloud
cloud:util.hpp cloud.cc 
	g++ -o $@ $^ -std=c++14 -L./lib -lpthread -lstdc++fs -ljsoncpp -lbundle
.PHONY:clean
clean:
	rm -f cloud
