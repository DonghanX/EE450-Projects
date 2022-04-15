all: compile run_crc_tx run_crc_rx run_crc_checksum
compile:
	g++ -std=c++0x -o crc_tx crc_tx.cpp
	g++ -std=c++0x -o crc_rx crc_rx.cpp
	g++ -std=c++0x -o crc_vs_checksum crc_vs_checksum.cpp

run_crc_tx:
	./crc_tx
run_crc_rx:
	./crc_rx
run_crc_checksum:
	./crc_vs_checksum
clean:
	rm crc_tx crc_rx crc_vs_checksum
	
