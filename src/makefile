.PHONY:cloud
cloud:cloud.cc 
	g++ -g -o $@ $^ -std=c++14 -L./lib -lpthread -lstdc++fs -ljsoncpp -lbundle
.PHONY:clean
clean:
	rm -f cloud

# .PHONY: cloud
# cloud:cloud.cc  util.hpp
# 	g++ -g $^ -o $@ -L./lib -lpthread -lstdc++fs -ljsoncpp -lbundle