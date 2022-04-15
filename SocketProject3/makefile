all: serverA serverB servermain client
servermain: servermain.cpp 
	g++ -std=c++0x -o servermain servermain.cpp
serverA: serverA.cpp 
	g++ -std=c++0x -o serverA serverA.cpp
serverB: serverB.cpp
	g++ -std=c++0x -o serverB serverB.cpp
client: client.cpp 
	g++ -std=c++0x -o client client.cpp

clean:
	rm serverA serverB servermain client
