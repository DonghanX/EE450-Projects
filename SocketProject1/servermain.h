#include <iostream>

void ReadListInfo(
    std::string, 
    std::map<std::string, std::string>&, 
    std::vector<std::string>&
);

void SplitCityList(
    std::string, 
    char, 
    std::string, 
    std::map<std::string, std::string>&
);

void InsertCityStateElement(
    std::string, 
    std::string, 
    std::map<std::string, std::string>&
);

std::string GetAllStateNames(std::vector<std::string>&);

std::string QueryStateByCity(
    std::string, 
    std::map<std::string, std::string>&,
    std::string&
);

addrinfo AssembleHints();

void BootupServer();

void RetrieveAllAddrInfo(addrinfo**, const addrinfo&);

void ReusePortIfNeeded(int);

void BindSocket(int, addrinfo*);

void RetrieveValidAddrInfo(addrinfo**, addrinfo, int&);

int GetSocketFd(addrinfo*);

void ListenOnSocket(int, int);

void AcceptConnection(int, std::map<std::string, std::string>&, std::string&);

void ReceiveFromClient(int, std::vector<char>&, std::string&, int);

std::string GetResponseContent(std::vector<char>&);

void SendToClient(int, std::string&, std::string, int);

int GetClientPortNumber(int socket_fd);

void PrintRecvContent(int, std::string&, int);