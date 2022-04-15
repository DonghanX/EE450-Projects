#include <iostream>

void BootupServer();

void RetrieveAllAddrInfo(addrinfo**, const addrinfo&, std::string);

void BindSocket(int, addrinfo*);

void RetrieveValidAddrInfo(addrinfo**, addrinfo, std::string, int&, bool);

int GetSocketFd(addrinfo*);

addrinfo AssembleHints(bool);

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

void ProcessQuery(
    int,
    addrinfo**, 
    addrinfo*,
    std::pair<std::string, std::string>&, 
    std::map<std::string, char>&, 
    std::string&, 
    char&
);

int ConvertBackendIdIntoIndex(char);

void ListStateResponsibility(
    std::map<std::string, char>&
);

void SendResultToClient(
    int, 
    addrinfo*, 
    int, 
    char, 
    std::pair<std::string, std::string>&, 
    std::string&
);

std::pair<std::string, std::string> RetrieveStateAndUserIdFromComboData(
    std::string, 
    char
);

void ListenOnSocket(int, int);

void AcceptConnection(
    int,
    int, 
    addrinfo**, 
    addrinfo*,
    addrinfo*, 
    std::map<std::string, char>&
);

void ReceiveFromClient(int, std::vector<char>&, std::string&, int);
