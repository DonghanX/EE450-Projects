#include<iostream>

addrinfo AssembleHints();

void BootupClient();

void RetrieveAllAddrInfo(addrinfo**, const addrinfo&);

void RetrieveValidAddrInfo(addrinfo**, addrinfo, int&);

int GetSocketFd(addrinfo*);

void GetInputCityName(std::string&);

std::string GetResponseContent(std::vector<char>&);

int SendToServer(int, std::string);

void PrintRecvContent(std::string&, std::string&);

void ReceiveFromServer(int, std::vector<char>&, std::string&);

int GetClientPortNumber(int);


