#include<iostream>

addrinfo AssembleHints();

void BootupClient();

void RetrieveAllAddrInfo(addrinfo**, const addrinfo&);

void RetrieveValidAddrInfo(addrinfo**, addrinfo, int&);

int GetSocketFd(addrinfo*);

std::string GetResponseContent(std::vector<char>&);

int SendToServer(int, const std::pair<std::string, std::string>&, char);

void PrintRecvContent(std::string&, const std::pair<std::string, std::string>&);

void ReceiveFromServer(int, std::vector<char>&, std::string&);

int GetClientPortNumber(int);

void InputComboData(std::pair<std::string, std::string>&);

int GetLocalPortNumber(addrinfo*);

int GetLocalPortNumber(int);

void AppendUserPrefixToEachUserId(std::string&, std::string, char);
