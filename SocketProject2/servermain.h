#include <iostream>

void BootupServer();

void RetrieveAllAddrInfo(addrinfo**, const addrinfo&, std::string);

void BindSocket(int, addrinfo*);

void RetrieveValidAddrInfo(addrinfo**, addrinfo, std::string, int&, bool);

int GetSocketFd(addrinfo*);

addrinfo AssembleHints();

void ReusePortIfNeeded(int);

void SendToBackend(int, addrinfo*, std::string);

void ReceiveFromBackend(int, std::string&);

std::string ConvertReceivedContent(std::vector<char>&);

void RequestStateListFromBackend(
    int, 
    addrinfo*, 
    addrinfo*, 
    std::map<std::string, char>&, 
    char
);

void StoreStateResponsibility(
    std::string, 
    std::map<std::string, char>&, 
    char, 
    char
);

int GetLocalPortNumber(addrinfo*);

void GetInputStateName(std::string&);

void ProcessQuery(
    int, 
    addrinfo**, 
    addrinfo*, 
    std::map<std::string, char>&
);

int ConvertBackendIdIntoIndex(char);

int CountDistinctCityNumberInResult(std::string, char);

void ListStateResponsibility(
    std::map<std::string, char>&
);