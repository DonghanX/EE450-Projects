all: servermain client
servermain: servermain.cpp  
	g++ -std=c++0x -o servermain servermain.cpp
client: client.cpp 
	g++ -std=c++0x -o client client.cpp
run_server:
	./servermain
run_client:
	./client
clean:
	rm servermain client
	
