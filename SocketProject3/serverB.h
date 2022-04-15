#include <iostream>

void BootupServer();

void RetrieveAllAddrInfo(addrinfo**, const addrinfo&, std::string);

void BindSocket(int, addrinfo*);

void RetrieveValidAddrInfo(addrinfo**, addrinfo, std::string, int&, bool);

int GetSocketFd(addrinfo*);

addrinfo AssembleHints();

void ReusePortIfNeeded(int);

void ReadListInfo(
    std::string, 
    std::multimap<std::string, std::vector<std::string>>&, 
    std::vector<std::string>&
);

std::string GenerateRecommendationUsers(
    std::string, 
    std::string, 
    const std::multimap<std::string, std::vector<std::string>>&
);

std::string GenerateDelimitedStringFromSet(
    const std::set<std::string>&, 
    char delimiter
);

void PrintRecommendationResult(
    std::string, 
    std::string, 
    std::string&
);

void InsertStateGroupElement(
    std::string, 
    std::vector<std::string>&, 
    std::multimap<std::string, std::vector<std::string>>&
);

void SendToMainServer(int, addrinfo*, std::string);

void ReceiveFromMainServer(int, std::string&);

std::string ConvertReceivedContent(std::vector<char>&);

void ReplyStateResponsibilityToMainServer(
    int, 
    addrinfo*, 
    std::vector<std::string>&
);

void SplitUserList(
    std::string, 
    char, 
    std::vector<std::string>&
);

void GenerateRecommendationSet(
    std::string, 
    std::vector<std::string>&, 
    std::set<std::string>&
);

std::string GenerateLocalResponsibleStateList(
    std::vector<std::string>&, 
    char
);

std::pair<std::string, std::string> RetrieveStateAndUserIdFromComboData(
    std::string, 
    char
);

int GetLocalPortNumber(addrinfo*);

void ProcessQueryFromMainServer(
    int, 
    addrinfo*, 
    std::multimap<std::string, std::vector<std::string>>&
);



