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
    std::map<std::string, std::set<std::string>>&, 
    std::vector<std::string>&
);

void SplitCityList(
    std::string, 
    char, 
    std::string,  
    std::map<std::string, std::set<std::string>>&
);

void InsertCityStateElement(
    std::string, 
    std::set<std::string>&, 
    std::map<std::string, std::set<std::string>>&
);

void SendToMainServer(int, addrinfo*, std::string);

void ReceiveFromMainServer(int, std::string&);

std::string ConvertReceivedContent(std::vector<char>&);

void ReplyStateResponsibilityToMainServer(
    int, 
    addrinfo*, 
    std::vector<std::string>&
);

void QueryCitiesByState(
    std::string,  
    std::map<std::string, std::set<std::string>>&, 
    std::string &result_list, 
    int &city_num
);

std::string GetLocalResponsibleStateList(
    std::vector<std::string>&, 
    char
);

int GetLocalPortNumber(addrinfo*);

void ProcessQueryFromMainServer(
    int, 
    addrinfo*, 
    std::map<std::string, std::set<std::string>>
);



