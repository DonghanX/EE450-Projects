#include <iostream>
#include <string>
#include <cstring>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "client.h"

// IP address of localhost
#define LOCALHOST "127.0.0.1"
// 451 is the last 3 digits of my USC ID
#define SERVER_PORT "33451"

// faliure flag
#define SOCKET_FD_FAILURE -1
#define SOCKET_OPTION_FAILURE -1
#define BIND_FAILURE -1
#define CONNECT_FALIURE -1
#define SEND_FAILURE -1
#define RECEIVE_FAILURE -1
#define EMPTY_CONTENT_FALG -1

// city-name-not-found identifier
#define NOT_FOUND_CONTENT "Not Found"

/**
 * @description: bootup client to prepare for connecting to localhost
 * @param {*}
 * @return {*}
 */
void BootupClient() {
    addrinfo *valid_addr_info;
    int socket_fd;

    std::string city_name;
    std::string recv_content;

    RetrieveValidAddrInfo(&valid_addr_info, AssembleHints(), socket_fd);

    while (true) {
        // use vector to serve as a buffer container instead of char[] to slightly improve
        // ... proformance and make the code more C++ style
        std::vector<char> buffer(4096);

        GetInputCityName(city_name);
        // receive content only if content to be sent is not empty
        if (SendToServer(socket_fd, city_name) != EMPTY_CONTENT_FALG) {
            ReceiveFromServer(socket_fd, buffer, recv_content);

            PrintRecvContent(recv_content, city_name);
        }
    }
}

/**
 * @description: send contents to localhost via socket file descriptor
 * @param {int} socket_fd
 * @param {string} content
 * @return {int} status code that returns -1 when the sending content is empty and 0 if
 *              ... succeeds
 */
int SendToServer(int socket_fd, std::string content) {
    int status_code;

    // prevent empty content from being sent
    if (content.empty()) {
        return -1;
    }
    status_code = send(socket_fd, content.c_str(), content.size(), 0);

    // if (status_code == SEND_FAILURE) {
    //     std::cout << "Send Failed" << std::endl;
    // }

    std::cout << "Client has send city "
        << content
        << " to Main Server using TCP."
        << std::endl;

    return 0;
}

/**
 * @description: receive contents from localhost via socket file descriptor
 * @param {int} socket_fd
 * @param {vector<char>} &buffer
 * @param {string} &content
 * @return {*}
 */
void ReceiveFromServer(int socket_fd, std::vector<char> &buffer, std::string &content) {
    int recv_length;
    
    // use vector::data() to get a direct pointer to the continuous memory array used by vector buffer
    // it is equal to &buffer[0]
    recv_length = recv(socket_fd, buffer.data(), buffer.size(), 0);

    if (recv_length != RECEIVE_FAILURE) {
        // reallocate the buffer to reduce the memory usage
        buffer.resize(recv_length);
        content = GetResponseContent(buffer);
    }

}

/**
 * @description: get the input city name that might contains whitespace, i.e. "Los Angeles"
 * @param {string} &city_name
 * @return {*}
 */
void GetInputCityName(std::string &city_name) {
    std::cout << "Enter City Name: ";
    // replace cin with getLine to avoid missing the invisible character
    std::getline(std::cin, city_name);
}

/**
 * @description: convert all chars in buffer vector to a string obj
 * @param {vector<char>} &buffer
 * @return {string}
 */
std::string GetResponseContent(std::vector<char> &buffer) {
    std::string content;
    for (char single_char: buffer) {
        content += single_char;
    }
    
    return content;
}

/**
 * @description: print the contents received in predefined format according to project requirements
 * @param {string} &content
 * @param {string} &city_name
 * @return {*}
 */
void PrintRecvContent(std::string &content, std::string &city_name) {
    if (content == NOT_FOUND_CONTENT) {
        std::cout << city_name << " " << content << std::endl;
    } else {
        std::cout << "Client has received results from Main Server:" << std::endl;
        std::cout << city_name << " is associated with state " << content << std::endl;
    }

    std::cout << "-----Start a new query-----" << std::endl;
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
    memset(&hints, 0, sizeof hints);
    // either ipv4 or ipv6 is ok
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    // use the returned socket address struct to the incoming bind() procedure
    hints.ai_flags = AI_PASSIVE;

    return hints;
}

/**
 * @description: retrieve the linked list of struct addrinfo that is filled out by getaddrinfo() 
 *              ... with all kinds of address information
 * @reference: Section 5.1, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {addrinfo**} result_addr_info, double pointers of address of the linked list of address
 *              ... info that will be used to pass the pointer of unchecked addressinfo out of 
 *              ... local function RetrieveAllAddrInfo() 
 * @param {addrinfo} &hints, pass-by-reference of unchanged hints address information
 * @return {*}
 */
void RetrieveAllAddrInfo(addrinfo **result_addr_info, const addrinfo &hints) {
    addrinfo *assigned_addr_info;
    // assign all kinds of address information to *assigned_addr_info
    int status_code = getaddrinfo(LOCALHOST, SERVER_PORT, &hints, &assigned_addr_info);
    if (status_code != NETDB_SUCCESS) {
        std::cout << gai_strerror(status_code) << std::endl;
        exit(EXIT_FAILURE);
    }
    // make *result_addr_info points to the address of the linked list of unchecked addressinfo, 
    // ... thus **result_addr_info will points to the actual value of assigned_addr_info
    *result_addr_info = assigned_addr_info;
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
 * @description: retrieve the valid addressinfo by iterating linked list of addressinfo
 *              ... and check whether the current addressinfo could be used for create
 *              ... socket file descriptor and for bind()
 * @reference: Section 5.1, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {addrinfo**} **valid_addr_info, double pointers of the address of iteration 
 *              ... addressinfo that will be used to pass the pointer of valid address 
 *              ... info out of local function RetrieveValidAddrinfo() 
 * @param {int&} valid socket file descriptor
 * @return {void}
 */
void RetrieveValidAddrInfo(
    addrinfo **valid_addr_info, 
    addrinfo hints, 
    int &socket_fd
) {
    addrinfo *iter;
    // retrieve all kinds of addressinfo and make iter be the pointer of linked
    // ... list of unchecked addressinfo
    RetrieveAllAddrInfo(&iter, hints);
    // find the valid addressinfo
    while (iter != NULL) {
        // generate socket file descriptor using the value from the result of getaddrinfo()
        // ... no matter whether the result addressinfo is valid
        // we will later check the validation of socket file descriptor to find the valid
        // ... addressinfo
        socket_fd = GetSocketFd(iter);
        if (socket_fd == SOCKET_FD_FAILURE) {
            // directly jump to next iteration to find the valid addressinfo that creates 
            // ... socket file descriptor correctly
            iter = iter->ai_next;
            continue;
        }

        // start connecting to the server
        if (connect(socket_fd, iter->ai_addr, iter->ai_addrlen) == CONNECT_FALIURE) {
            // directly jump to next iteration to find the valid addressinfo that could be
            // ... used in starting connection
            close(socket_fd);

            iter = iter->ai_next;
            continue;
        }

        break;
    }

    *valid_addr_info = iter;
    
    // deallocate memory of linked list of unchecked addressinfo
    freeaddrinfo(iter);

    std::cout << "Client is up and running" << std::endl;
}

int main() {

    BootupClient();

    return 0;
}
