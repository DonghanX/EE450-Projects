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
#include <list>
#include <set>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <mutex>

#include "servermain.h"

// delimiter that split the city in each even row in list file
#define CITY_DELIMITER ','
// delimiter that split the state in backend's response
#define STATE_DELIMITER ','
// IP address of localhost
#define LOCALHOST "127.0.0.1"
// static port number of localhost
// 451 is the last 3 digits of my USC ID
#define SERVER_A_PORT "30451"
#define SERVER_B_PORT "31451"
#define SERVER_MAIN_PORT "32451"
// pre-defined signal for main server to ask backend servers for responsibility list of states
#define RESPONSIBLE_REQUEST_CONTENT "##request##"
// pre-defined backend server id
#define SERVER_A_ID 'A'
#define SERVER_B_ID 'B'

// failue flag
#define SOCKET_FD_FAILURE -1
#define SOCKET_OPTION_FAILURE -1
#define BIND_FAILURE -1
#define RECEIVE_FAILURE -1
#define SEND_FAILURE -1

void BootupServer() {
    std::map<std::string, char> state_backend_map;

    // local addrinfo
    addrinfo *local_addr_info;
    // addrinfo of remote host
    addrinfo *backend_A_addr_info;
    addrinfo *backend_B_addr_info;

    // socket file descriptor for the socket used in recvfrom() and sendto()
    int socket_fd;

    RetrieveValidAddrInfo(&local_addr_info, AssembleHints(), SERVER_MAIN_PORT, socket_fd, true);

    // bind local addrinfo to local socket file descriptor to assign a certain address and 
    // port number to serverMain, which can be utilized in receiving connectionless UDP datagram
    BindSocket(socket_fd, local_addr_info);

    RetrieveValidAddrInfo(&backend_A_addr_info, AssembleHints(), SERVER_A_PORT, socket_fd, false);
    RetrieveValidAddrInfo(&backend_B_addr_info, AssembleHints(), SERVER_B_PORT, socket_fd, false);
    ReusePortIfNeeded(socket_fd);

    std::cout << "Main server is up and running" << std::endl;

    // ask backend server for their responsibilities for corresponding states
    RequestStateListFromBackend(socket_fd, backend_A_addr_info, local_addr_info, state_backend_map, SERVER_A_ID);
    RequestStateListFromBackend(socket_fd, backend_B_addr_info, local_addr_info, state_backend_map, SERVER_B_ID);

    ListStateResponsibility(state_backend_map);


    // start query
    // use pointer array to store all addrinfo pointers of backend servers
    addrinfo* addr_info_array[2];
    addr_info_array[0] = backend_A_addr_info;
    addr_info_array[1] = backend_B_addr_info;

    while (true) {
        ProcessQuery(socket_fd, addr_info_array, local_addr_info, state_backend_map);
        std::cout << "-----Start a new query-----" << std::endl;
    }

    // deallocate memory of linked list of unchecked addressinfo
    freeaddrinfo(backend_A_addr_info);
    freeaddrinfo(backend_B_addr_info);
    freeaddrinfo(local_addr_info);
    
    close(socket_fd);
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
    
    // deallocate memory of linked list of unchecked addressinfo
    // freeaddrinfo(iter);
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
 * @description: receive contents via UDP socket file descriptor
 * @reference: Section 5.8, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {int} socket_fd
 * @param {string} &recv_content
 * @return {*}
 */
void ReceiveFromBackend(int socket_fd, std::string &recv_content) {
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
        std::cout << "received empty content" << std::endl;
        recv_content = "";
        return;
    }

    if (recv_length != RECEIVE_FAILURE) {
        // reallocate the buffer to reduce the memory usage
        buffer.resize(recv_length);
        recv_content = ConvertReceivedContent(buffer);

        // PrintRecvContent(socket_fd, recv_content, backend_id);
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
void SendToBackend(int socket_fd, addrinfo *valid_addr_info, std::string content) {
    sockaddr_storage sender_addr_storage;
    socklen_t addr_length = sizeof(sender_addr_storage);
    // cast sockaddr_storage to sockaddr address to serve as params in recvfrom
    sockaddr *sender_addr = (sockaddr*) &sender_addr_storage;

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
 * @description: get the input state name and send it to the corresponding backend server
 * @param {int} socket_fd
 * @param {addrinfo**} addr_info_array, array that stores all the backend server addrinfo*
 * @param {addrinfo*} local_addr_info
 * @param {map<std::string, int>&}
 * @return {*}
 */
void ProcessQuery(
    int socket_fd, 
    addrinfo **addr_info_array, 
    addrinfo *local_addr_info, 
    std::map<std::string, char>& state_backend_map
) {
    char backend_id;
    // index in backend addrinfo array
    int backend_index;
    int port_num;
    std::string state_name;
    std::string result_list;

    GetInputStateName(state_name);

    // find which server is responsible for the input state name
    // firstly we check if the input state does not belong to any backend server
    // since all elements in a map are unique, the map::count() can only return 1
    // ... if the key exists
    if (!state_backend_map.count(state_name)) {
        std::cout << state_name << " does not show up in server A&B" << std::endl;
        return;
    }
    backend_id = state_backend_map[state_name];
    std::cout << state_name << " shows up in server " << backend_id << std::endl;

    backend_index = ConvertBackendIdIntoIndex(backend_id);
    // send the input state name to corresponding backend server
    SendToBackend(socket_fd, addr_info_array[backend_index], state_name);

    port_num = GetLocalPortNumber(local_addr_info);
    std::cout << "The Main Server has sent request for " 
        << state_name
        << " to server " << backend_id
        << " using UDP over port " << port_num
        << std::endl;

    // receive result sent from backend server
    ReceiveFromBackend(socket_fd, result_list);

    std::cout << "The Main server has received searching result(s) of "
        << state_name
        << " from server " << backend_id
        << std::endl;

    std::cout << "There are " << CountDistinctCityNumberInResult(result_list, CITY_DELIMITER)
        << " dinstinct cities in " 
        << state_name << ": "
        << result_list
        << std::endl;
}

/**
 * @description: retrieve number of distinct cities by counting the number of delimiter
 *              ... so that the number of cities is equal to the number of delimiter plus 1
 * @param {string} result_list
 * @param {char} delimiter
 * @return {*}
 */
int CountDistinctCityNumberInResult(std::string result_list, char delimiter) {
    int position;
    int delimiter_num = 0;
    while (true) {
        // find the position of delimiter
        position = result_list.find(delimiter);

        // upper-bound break condition
        if (position == result_list.npos) {
            break;
        }
        if (position >= result_list.size()) {
            break;
        }

        delimiter_num++;
        // remove the city name that has been extracted to make the std::string::find()
        // ... function could find the next delimiter instead of the same one
        result_list = result_list.substr(position + 1);
    }
    return ++delimiter_num;
}

/**
 * @description: convert backend unique identifier to the integer index, which could be
 *              ... utilized as the index of addrinfo* array
 * @param {char} backend_id
 * @return {*}
 */
int ConvertBackendIdIntoIndex(char backend_id) {
    // let A corresponds to 0, B corresponds to 1 ...
    return (int)(backend_id - 65);
}

void GetInputStateName(std::string &state_name) {
    std::cout << "Enter state name:";
    std::getline(std::cin, state_name);
}

/**
 * @description: send pre-defined signal to backend server to ask for their state
 *              ... responsibility
 * @param {int} socket_fd
 * @param {addrinfo*} addr_info
 * @param {char} backend_id
 * @return {*}
 */
void RequestStateListFromBackend(
    int socket_fd, 
    addrinfo* remote_addr_info, 
    addrinfo* local_addr_info, 
    std::map<std::string, char>& state_backend_map, 
    char backend_id
) {
    std::string state_list;
    SendToBackend(socket_fd, remote_addr_info, RESPONSIBLE_REQUEST_CONTENT);

    ReceiveFromBackend(socket_fd, state_list);

    if (!state_list.empty()) {
        StoreStateResponsibility(state_list, state_backend_map, STATE_DELIMITER, backend_id);
        // std::cout << "received:" << state_list << std::endl;
    }

    std::cout << "Main server has received the state list from server "
        << backend_id
        << " using UDP over port "
        << GetLocalPortNumber(local_addr_info)
        << std::endl;
}

/**
 * @description: store the received responsibility information using red-black tree map
 * @param {string} state_list
 * @param {char} delimiter
 * @param {char} backend_id
 * @return {*}
 */
void StoreStateResponsibility(
    std::string state_list, 
    std::map<std::string, char>& state_backend_map, 
    char delimiter, 
    char backend_id
) {
    int position;
    std::string state_name;

    while (true) {
        // find the position of delimiter
        position = state_list.find(delimiter);

        // upper-bound break condition
        if (position == state_list.npos) {
            // notice that state list may not contain delimiter or the position is correspond to
            // ... the start of the last state in the state list
            // so we simply extract the whole content of the remaining state list, which refers 
            // ... to one certain state name
            state_backend_map.insert(std::make_pair(state_list, backend_id));
            break;
        }
        if (position >= state_list.size()) {
            break;
        }
        // extract substring that refers to a certain state name according to the two delimiters
        state_name = state_list.substr(0, position);
        state_backend_map.insert(std::make_pair(state_name, backend_id));

        // remove the state name that has been extracted
        state_list = state_list.substr(position + 1);
    }
}

/**
 * @description: list the results of which states the two backend server is responsible for
 * @param {map<std::string, char>} &state_backend_map
 * @return {*}
 */
void ListStateResponsibility(std::map<std::string, char> &state_backend_map) {
    std::string backend_A_responsibility;
    std::string backend_B_responsibility;

    std::map<std::string, char>::iterator iter = state_backend_map.begin();

    for (; iter != state_backend_map.end(); iter++) {
        if (iter->second == SERVER_A_ID) {
            backend_A_responsibility += iter->first;
            backend_A_responsibility += '\n';
        } else {
            backend_B_responsibility += iter->first;
            backend_B_responsibility += '\n';
        }
    }
    
    std::cout << "Server " << SERVER_A_ID << std::endl
        << backend_A_responsibility 
        << std::endl;

    std::cout << "Server " << SERVER_B_ID << std::endl
        << backend_B_responsibility 
        << std::endl;

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

