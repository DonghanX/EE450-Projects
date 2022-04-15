all: serverA serverB servermain 
serverA: serverA.cpp 
	g++ -std=c++0x -o serverA serverA.cpp
serverB: serverB.cpp
	g++ -std=c++0x -o serverB serverB.cpp
servermain: servermain.cpp
	g++ -std=c++0x -o serverMain servermain.cpp


clean:
	rm serverA serverB serverMain

