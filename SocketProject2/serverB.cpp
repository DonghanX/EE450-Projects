#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <functional>
#include <map>
#include <vector>
#include <iterator>
#include <set>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <mutex>

#include "serverB.h"

// delimiter that split the city in each even row in list file
#define CITY_DELIMITER ','
// delimiter that split the state in backend's response
#define STATE_DELIMITER ','
// file name of city-state mapping file
#define LIST_FILE_NAME "dataB.txt"
//
#define SERVER_ID 'B'
// IP address of localhost
#define LOCALHOST "127.0.0.1"
// static port number of localhost
// 451 is the last 3 digits of my USC ID
#define BACKEND_SERVER_PORT "31451"
#define SERVER_MAIN_PORT "32451"
// pre-defined signal for main server to ask backend servers for responsibility list of states
#define RESPONSIBLE_REQUEST_CONTENT "##request##"


// failue flag
#define SOCKET_FD_FAILURE -1
#define SOCKET_OPTION_FAILURE -1
#define BIND_FAILURE -1
#define RECEIVE_FAILURE -1
#define SEND_FAILURE -1

void BootupServer() {

    // use map to store the city-state information, where the key is state name and the value
    // ... is the set of distinct city name corresponding to the state name
    std::map<std::string, std::set<std::string>> state_city_map;
    // use vector to store state-only information, which will be uesd in printing all of state
    // ... names if the input city name could not be found
    std::vector<std::string> state_vector;
    ReadListInfo(LIST_FILE_NAME, state_city_map, state_vector);

    std::map<std::string, std::set<std::string>>::iterator iter = state_city_map.begin();

    // local addrinfo
    addrinfo *local_addr_info;
    // addrinfo of remote host
    addrinfo *remote_addr_info;
    // socket file descriptor for the socket used in recvfrom() and sendto()
    int socket_fd;
    RetrieveValidAddrInfo(&local_addr_info, AssembleHints(), BACKEND_SERVER_PORT, socket_fd, true);

    // bind local addrinfo to local socket file descriptor to assign a certain address and 
    // port number to serverA, which can be utilized in receiving connectionless UDP datagram
    BindSocket(socket_fd, local_addr_info);

    RetrieveValidAddrInfo(&remote_addr_info, AssembleHints(), SERVER_MAIN_PORT, socket_fd, false);

    ReusePortIfNeeded(socket_fd);

    std::cout << "Server " << SERVER_ID
        << " is up and running using UDP on port "
        << GetLocalPortNumber(local_addr_info)
        << std::endl;

    ReplyStateResponsibilityToMainServer(socket_fd, remote_addr_info, state_vector);

    while (true) {
        ProcessQueryFromMainServer(socket_fd, remote_addr_info, state_city_map);
    }

    // deallocate memory of linked list of unchecked addressinfo
    freeaddrinfo(remote_addr_info);
    freeaddrinfo(local_addr_info);

    close(socket_fd);
}

/**
 * @description: receive the query request from main server and replay it with the cities corresponding
 *              ... to the state
 * @param {int} socket_fd
 * @param {addrinfo*} remote_addr_info
 * @param {map<string, set<string>>} state_city_map
 * @return {*}
 */
void ProcessQueryFromMainServer(
    int socket_fd, 
    addrinfo* remote_addr_info, 
    std::map<std::string, std::set<std::string>> state_city_map
) {
    std::string state_name;
    std::string result_list;
    int distinct_city_num;
    // receive query from main server
    ReceiveFromMainServer(socket_fd, state_name);
    
    std::cout << "Server " << SERVER_ID
        << " has reeived a request for "
        << state_name
        << std::endl;

    // query the state-city-map to find all distinct cities corresponding to the state
    QueryCitiesByState(state_name, state_city_map, result_list, distinct_city_num);

    std::cout << "Server " << SERVER_ID
        << " found " << distinct_city_num
        << " distinct cities for "
        << state_name << ": "
        << result_list
        << std::endl;

    // send the result list to main server
    SendToMainServer(socket_fd, remote_addr_info, result_list);
}

/**
 * @description: receive the state responsibility request from main server and reply it with
 *              ... the results of which states the current backend server is responsible for
 * @param {int} socket_fd
 * @param {addrinfo*} addr_info
 * @param {vector<std::string>} &state_vector
 * @return {*}
 */
void ReplyStateResponsibilityToMainServer(
    int socket_fd, 
    addrinfo* addr_info, 
    std::vector<std::string> &state_vector
) {
    std::string recv_content;
    std::string state_list;

    // check if received content is the pre-defined request singal
    ReceiveFromMainServer(socket_fd, recv_content);
    if (recv_content == RESPONSIBLE_REQUEST_CONTENT) {
        state_list = GetLocalResponsibleStateList(state_vector, STATE_DELIMITER);
        SendToMainServer(socket_fd, addr_info, state_list);
        
        std::cout << "Server "
            << SERVER_ID
            << " has sent a state list to Main Server"
            << std::endl;

    } else {
        std::cout << "backend server did not receive the request from main server" << std::endl;
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
}

/**
 * @description: collect all the strings in state_vector<string> and convert them into single string
 *              ... that strings up these contents with delimiter ','
 * @param {vector<string>} state_vector
 * @param {char} delimiter
 * @return {string} state list that contains all state names for which the backend server responsible
 */
std::string GetLocalResponsibleStateList(
    std::vector<std::string> &state_vector, 
    char delimiter
) {
    std::string content;
    std::vector<std::string>::iterator iter = state_vector.begin();

    for (; iter != state_vector.end(); iter++) {
        content += *iter;
        // append delimiter comma pairwisely
        if (iter != state_vector.end() - 1) {
            content += delimiter;
        }
    }

    return content;
}

/**
 * @description: pre-define some of the addrinfo parameters
 * @reference: Section 5.1, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {*}
 * @return {addrinfo} addrinfo with pre-defined parameters
 */
addrinfo AssembleHints() {
    addrinfo hints;

    // reset addrinfo struct to empty or the return value of getaddrinfo() might 
    // ... takes from some of the EAI_ERROR code
    memset(&hints, 0, sizeof(hints));
    // either ipv4 or ipv6 is ok
    hints.ai_family = AF_UNSPEC;
    // use UDP datagram socket type
    hints.ai_socktype = SOCK_DGRAM;
    // use the returned socket address struct to the incoming bind() procedure
    hints.ai_flags = AI_PASSIVE;

    return hints;
}

/**
 * @description: retrieve port number that is used to bind
 * @param {sockaddr} *socket_addr
 * @return {*}
 */
int GetLocalPortNumber(addrinfo *addr_info) {
    in_port_t port_num;
    sockaddr *socket_addr;

    socket_addr = (sockaddr*)addr_info->ai_addr;
    // consider both cases in IPv4 and IPv6
    if (socket_addr->sa_family == AF_INET) {
        port_num = ((sockaddr_in*)socket_addr)->sin_port;
    } else {
        port_num = ((sockaddr_in6*)socket_addr)->sin6_port;
    }
    // convert original port number from network byte order to host byte order
    return (int)ntohs(port_num);
}

/**
 * @description: retrieve the valid addressinfo by iterating linked list of addressinfo
 *              ... and check whether the current addressinfo could be used for create
 *              ... socket file descriptor and for bind()
 * @reference: Section 5.1, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {addrinfo**} **valid_addr_info, double pointers of the address of iteration 
 *              ... addressinfo that will be used to pass the pointer of valid address 
 *              ... info out of local function RetrieveValidAddrinfo() 
 * @param {int} port_number, port number that the UDP datagram should be sent to or receive
 *              .... from
 * @param {int&} valid socket file descriptor
 * @param {bool} needs_bind, flag that indicates whether the socket file descriptor has been created
 * @return {void}
 */
void RetrieveValidAddrInfo(
    addrinfo **valid_addr_info, 
    addrinfo hints,
    std::string port_number, 
    int &socket_fd, 
    bool needs_bind = true
) {
    addrinfo *iter;
    // retrieve all kinds of addressinfo and make iter be the pointer of linked
    // ... list of unchecked addressinfo
    RetrieveAllAddrInfo(&iter, hints, port_number);
    // find the valid addressinfo
    // if there have already been a UDP socket file descriptor created for sending/receiving, 
    // ... we will not need a new socket file descriptor for receiving/sending anymore.
    // ... Instead, we should use the same socket file descriptor for both sending and receiving.
    while (iter != NULL && needs_bind) {
        // generate socket file descriptor using the value from the result of getaddrinfo()
        // ... no matter whether the result addressinfo is valid
        // we will later check the validation of socket file descriptor to find the valid
        // ... addressinfo
        socket_fd = GetSocketFd(iter);
        if (socket_fd == SOCKET_FD_FAILURE) {
            // directly jump to next iteration to find the valid addressinfo
            iter = iter->ai_next;
            continue;
        }
        break;
    }

    *valid_addr_info = iter;

}

/**
 * @description: retrieve the linked list of struct addrinfo that is filled out by getaddrinfo() 
 *              ... with all kinds of address information
 * @reference: Section 5.1, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {addrinfo**} result_addr_info, double pointers of address of the linked list of address
 *              ... info that will be used to pass the pointer of unchecked addressinfo out of 
 *              ... local function RetrieveAllAddrInfo() 
 * @param {string} port_number
 * @param {addrinfo} &hints, pass-by-reference of unchanged hints address information
 * @return {*}
 */
void RetrieveAllAddrInfo(addrinfo **result_addr_info, const addrinfo &hints, std::string port_number) {
    addrinfo *assigned_addr_info;
    // assign all kinds of address information to *assigned_addr_info
    int status_code = getaddrinfo(LOCALHOST, port_number.c_str(), &hints, &assigned_addr_info);
    if (status_code != NETDB_SUCCESS) {
        std::cout << gai_strerror(status_code) << std::endl;
        exit(EXIT_FAILURE);
    }
    // make *result_addr_info points to the address of the linked list of unchecked addressinfo, 
    // ... thus **result_addr_info will points to the actual value of assigned_addr_info
    *result_addr_info = assigned_addr_info;
}

/**
 * @description: allow to reuse the port in case that "Address already in use" when calling bind()
 *              ... and terminate the main process if received error etatus code from
 *              ... setsockopt()
 * @reference: Section 5.3, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {int} socket_fd, socket file descriptor
 * @return {*}
 */
void ReusePortIfNeeded(int socket_fd) {
    int option_val = 1;
    int status_code;

    // option_value is used to access option values for setsocketopt() and identify a buffer
    status_code = setsockopt(
        socket_fd, SOL_SOCKET, SO_REUSEADDR, &option_val, sizeof(option_val));
    if (status_code == SOCKET_OPTION_FAILURE) {
        exit(EXIT_FAILURE);
    }
}

/**
 * @description: encapsulate socket()
 * @param {addrinfo*} addr_info
 * @return {int} socket file descriptor
 */
int GetSocketFd(addrinfo* addr_info) {
    return socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);
}

/**
 * @description: associate the socket file descriptor with a local address and port number
 * @param {int} socket_fd
 * @param {addrinfo} *addr_info
 * @return {*}
 */
void BindSocket(int socket_fd, addrinfo *addr_info) {
    int status_code;
    
    status_code = bind(socket_fd, addr_info->ai_addr, addr_info->ai_addrlen);
    if (status_code == BIND_FAILURE) {
        // deallocate the socket file descripter and terminate main process if failed in bind()
        std::cout << "Bind Failed" << std::endl;
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // std::cout << "start bind to: " << addr_info->ai_addr << std::endl;
}

/**
 * @description: receive contents via UDP socket file descriptor
 * @reference: Section 5.8, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {int} socket_fd
 * @param {string} &recv_content
 * @return {*}
 */
void ReceiveFromMainServer(int socket_fd, std::string &recv_content) {
    std::vector<char> buffer(4096);
    int recv_length;

    sockaddr_storage sender_addr_storage;
    socklen_t addr_length = sizeof(sender_addr_storage);
    // cast sockaddr_storage to sockaddr address to serve as params in recvfrom
    sockaddr *sender_addr = (sockaddr*) &sender_addr_storage;

    // use vector::data() to get a direct pointer to the continuous memory array used by vector buffer
    // it is equal to &buffer[0]
    recv_length = recvfrom(socket_fd, buffer.data(), buffer.size(), 0, sender_addr, &addr_length);

    // prevent server from receiving empty content, especially when the client is terminated by "Ctrl+C"
    // ... it will send empty content through connection, due to the ugly codes that should be revised
    // TODO: modify these ugly codes 
    if (recv_length == 0) {
        recv_content = "";
        std::cout << "receive empty content" << std::endl;
        return;
    }

    if (recv_length != RECEIVE_FAILURE) {
        // reallocate the buffer to reduce the memory usage
        buffer.resize(recv_length);
        recv_content = ConvertReceivedContent(buffer);

        return;

        // PrintRecvContent(socket_fd, recv_content, backend_id);
    } else {
        std::cout << "receive failed" << std::endl;
    }
    // close(socket_fd);
    // exit(EXIT_SUCCESS);
}

/**
 * @description: send contents via UDP socket file descriptor
 * @reference: Section 5.8, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {int} socket_fd
 * @param {addrinfo*} valid_addr_info
 * @param {string} &content
 * @return {*}
 */
void SendToMainServer(int socket_fd, addrinfo *valid_addr_info, std::string content) {
    sockaddr_storage sender_addr_storage;
    socklen_t addr_length = sizeof(sender_addr_storage);
    // cast sockaddr_storage to sockaddr address to serve as params in recvfrom
    sockaddr *sender_addr = (sockaddr*) &sender_addr_storage;

    // std::cout << "start to send content: " << content << std::endl;

    int status_code;
    status_code = sendto(
        socket_fd, 
        content.c_str(), 
        strlen(content.c_str()), 
        0, 
        valid_addr_info->ai_addr, 
        valid_addr_info->ai_addrlen
    );

    if (status_code == SEND_FAILURE) {
        std::cout << "Send Failed" << std::endl;
    }
}

/**
 * @description: read the info file each line and store the city-state mapping information
 * @param {string} file_name, file name of city-state information txt file
 * @param {map<std::string, std::string>} &state_city_map, map that stores city-state elements
 * @param {vector<std::string>} &state_vector, vector that stores state-only elements
 * @return {*}
 */
void ReadListInfo(
    std::string file_name, 
    std::map<std::string, std::set<std::string>> &state_city_map, 
    std::vector<std::string> &state_vector
) {
    std::ifstream infile_stream;

    infile_stream.open(file_name.c_str());
    // assume that each string in odd line is a state name and each string in even line
    // ... contains one or a serie of city names
    int line_num = 1;
    std::string state_name;
    // a string of cities associated to the same state with delimiter commas
    std::string city_list;

    while (infile_stream.peek() != EOF) {
        if (line_num % 2 != 0) {
            // assign current state name to prepare for the city-state element insertion
            // ... in city-state-map
            std::getline(infile_stream, state_name);

            // insert state-only element to the state vector
            state_vector.push_back(state_name);
            // std::cout << "find state: " << state_name << std::endl;
        } else {

            std::getline(infile_stream, city_list);
            // insert city-state element while splitting city list by delimiter
            SplitCityList(city_list, CITY_DELIMITER, state_name, state_city_map);
        }

        line_num++;
    }
}

/**
 * @description: divide a city list into single or multiple city names by delimiter comma 
 *              ... and gather all the city names that are in the same state in a set to 
 *              ... serve as a value to the state name key
 * @param {string} city_list, string that contains one or multiple city names with delimiter
 * @param {char} delimiter, character by which the function splits the string
 * @param {string} state_name
 * @param {map<std::string, std::string>} &state_city_map
 * @return {*}
 */
void SplitCityList(
    std::string city_list, 
    char delimiter, 
    std::string state_name,  
    std::map<std::string, std::set<std::string>> &state_city_map
) {
    int position;

    std::string city_name;
    // string set to store all the city names in a same state and ensure all the city names are
    // ... distinct
    std::set<std::string> distinct_city_set;

    while (true) {
        // find the position of delimiter
        position = city_list.find(delimiter);

        // upper-bound break condition
        if (position == city_list.npos) {
            // notice that city list may not contain delimiter or the position is correspond to
            // ... the start of the last city in the city list
            // so we simply extract the whole content of the remaining city list, which refers 
            // ... to one certain city name
            distinct_city_set.insert(city_list);

            break;
        }
        if (position >= city_list.size()) {
            break;
        }
        // extract substring that refers to a certain city name according to the two delimiters
        city_name = city_list.substr(0, position);
        // remove the city name that has been extracted
        city_list = city_list.substr(position + 1);
        distinct_city_set.insert(city_name);
    }

    InsertCityStateElement(state_name, distinct_city_set, state_city_map);
}

/**
 * @description: insert state-city_set element to the map to store the information
 * @param {string} state_name
 * @param {vector<string>} distinct_city_set
 * @param {map<std::string, std::string>} &state_city_map
 * @return {*}
 */
void InsertCityStateElement(
    std::string state_name, 
    std::set<std::string> &distinct_city_set, 
    std::map<std::string, std::set<std::string>> &state_city_map
) {
    state_city_map.insert(std::make_pair(state_name, distinct_city_set));
}

/**
 * @description: access the city names by finding the value of state name(as a key) in the map
 * @param {string} state_name
 * @param {map<std::string, set<string>>} &state_state_map
 * @param {string} &result_list, list of distinct cities corresponding to the state name
 * @param {int} &city_num, number of distinct cities
 * @return {string} city name list corresponding to state name
 */
void QueryCitiesByState(
    std::string state_name,  
    std::map<std::string, std::set<std::string>> &state_city_map, 
    std::string &result_list, 
    int &city_num
) {
    if (state_city_map.find(state_name) == state_city_map.end()) {
        std::cout << "Not Found" << std::endl;
        return;
    }

    result_list = "";
    // find value distinct city set by key state name
    std::set<std::string> distinct_city_set = state_city_map[state_name];
    city_num = distinct_city_set.size();

    std::set<std::string>::iterator iter;
    for (iter = distinct_city_set.begin(); iter != distinct_city_set.end();) {
        result_list += *iter;
        // notice that the iterator of set is a Biderectional Iterator. We can not simply
        // ... utilize basic operators with the iterator, such as distinct_city_set.end() - 1
        iter++;
        if (iter != distinct_city_set.end()) {
            // append delimiter comma pairwisely
            result_list += CITY_DELIMITER;
        }
    }
}

/**
 * @description: convert all chars in buffer vector to a string obj
 * @param {vector<char>} &buffer
 * @return {string}
 */
std::string ConvertReceivedContent(std::vector<char> &buffer) {
    std::string content;
    for (char single_char: buffer) {
        content += single_char;
    }
    
    // std::cout << "received content:" << content << std::endl;

    return content;
}

int main() {

    BootupServer();

    return 0;
}

