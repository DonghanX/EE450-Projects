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
// file name of city-state mapping file
#define LIST_FILE_NAME "list.txt"
// IP address of localhost
#define LOCALHOST "127.0.0.1"
// static port number of localhost
// 451 is the last 3 digits of my USC ID
#define SERVER_PORT "33451"
// backlog of queue
#define BACKLOG 5
// failue flag
#define SOCKET_FD_FAILURE -1
#define SOCKET_OPTION_FAILURE -1
#define BIND_FAILURE -1
#define LISTEN_FAILURE -1
#define ACCEPT_FAILURE -1
#define RECEIVE_FAILURE -1
#define SEND_FAILURE -1
// city-name-not-found identifier
#define NOT_FOUND_CONTENT "Not Found"

// incremental client ID that should be visible to changes from all processes
int global_client_id = 0;

/**
 * @description: read the info file each line and store the city-state mapping information
 * @param {string} file_name, file name of city-state information txt file
 * @param {map<std::string, std::string>} &city_state_map, map that stores city-state elements
 * @param {vector<std::string>} &state_vector, vector that stores state-only elements
 * @return {*}
 */
void ReadListInfo(
    std::string file_name, 
    std::map<std::string, std::string> &city_state_map, 
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

    std::cout << "Main server has read the state list from " << file_name << '.' << std::endl;

    while (infile_stream.peek() != EOF) {
        if (line_num % 2 != 0) {
            // assign current state name to prepare for the city-state element insertion
            // ... in city-state-map
            std::getline(infile_stream, state_name);
            // insert state-only element to the state vector
            state_vector.push_back(state_name);

            std::cout << state_name << ':' << std::endl;

        } else {
            std::getline(infile_stream, city_list);
            // insert city-state element while splitting city list by delimiter
            SplitCityList(city_list, CITY_DELIMITER, state_name, city_state_map);
        }

        line_num++;
    }

}

/**
 * @description: divide a city list into single or multiple city names by delimiter comma 
 *              ... and insert city-state element to the map
 * @param {string} city_list, string that contains one or multiple city names with delimiter
 * @param {char} delimiter, character by which the function splits the string
 * @param {string} state_name
 * @param {map<std::string, std::string>} &city_state_map
 * @return {*}
 */
void SplitCityList(
    std::string city_list, 
    char delimiter, 
    std::string state_name,  
    std::map<std::string, std::string> &city_state_map
) {
    int position;

    std::string city_name;
    while (true) {
        // find the position of delimiter
        position = city_list.find(delimiter);

        // upper-bound break condition
        if (position == city_list.npos) {
            // notice that city list may not contain delimiter or the position is correspond to
            // ... the start of the last city in the city list
            // so we simply extract the whole content of the remaining city list, which refers 
            // ... to one certain city name
            InsertCityStateElement(state_name, city_list, city_state_map);

            break;
        }
        if (position >= city_list.size()) {
            break;
        }

        // extract substring that refers to a certain city name according to the two delimiters
        city_name = city_list.substr(0, position);

        std::cout << city_name << std::endl;

        // remove the city name that has been extracted
        city_list = city_list.substr(position + 1);

        // insert city-state element to the map
        InsertCityStateElement(state_name, city_name, city_state_map);
    }
}

/**
 * @description: prepare all-state-name string for printing certain on-screen messages
 * @param {vector} state_vector
 * @return {string} all state names with delimiter comma pairwise(according to requirement of 
 *          ... on-screen messages)
 */
std::string GetAllStateNames(std::vector<std::string> &state_vector) {
    std::string state_list;

    std::vector<std::string>::iterator iter;
    for (iter = state_vector.begin(); iter != state_vector.end(); iter++) {
        state_list += *iter;
        
        if (iter != state_vector.end() - 1) {
            // append delimiter comma pairwisely
            state_list += ',';
        }
    }
    // std::cout << "state number: " << state_vector.size() << std::endl;
    // std::cout << "state list" << state_list << std::endl;

    return state_list;
}

/**
 * @description: insert city-state element to the map to store the information
 * @param {string} state_name
 * @param {string} city_name
 * @param {map<std::string, std::string>} &city_state_map
 * @return {*}
 */
void InsertCityStateElement(
    std::string state_name, 
    std::string city_name, 
    std::map<std::string, std::string> &city_state_map
) {
    city_state_map.insert(std::make_pair(city_name, state_name));
    // std::cout << "state: " << state_name << '\t' << "city: " << city_name << std::endl;
}

/**
 * @description: access the state name by finding the value of city name (as a key) in the map
 * @param {string} city_name
 * @param {map<std::string, std::string>} &city_state_map
 * @param {string} state_list
 * @return {string} state name correspond to city name or not-found identifier if no matched state
 */
std::string QueryStateByCity(
    std::string city_name, 
    std::map<std::string, std::string> &city_state_map, 
    std::string &state_list
) {
    std::string state_name = city_state_map[city_name];
    // check if the input city name could be found in the map
    if(state_name.empty()) {
        std::cout << city_name << " does not show up in states "
            << state_list
            << std::endl;

        return NOT_FOUND_CONTENT;
    } else {
        std::cout << city_name << " is associated with state "
            << state_name
            << std::endl;

        return state_name;
    }
}

/**
 * @description: store city-state information and bootup server to prepare for incoming connections
 * @param {*}
 * @return {*}
 */
void BootupServer() {
    // use map to store the city-state information, where the key is city name and the value
    // ... is state name
    std::map<std::string, std::string> city_state_map;
    // all-state-name string 
    std::string state_list;
    // use vector to store state-only information, which will be uesd in printing all of state
    // ... names if the input city name could not be found
    std::vector<std::string> state_vector;

    ReadListInfo(LIST_FILE_NAME, city_state_map, state_vector);
    state_list = GetAllStateNames(state_vector);

    // addressinfo that can be used in getting socket file descriptor and in socket bind
    addrinfo *valid_addr_info;
    // socket file descriptor for the socket used in listen() in the main process
    int socket_fd;
    RetrieveValidAddrInfo(&valid_addr_info, AssembleHints(), socket_fd);

    ReusePortIfNeeded(socket_fd);

    BindSocket(socket_fd, valid_addr_info);

    ListenOnSocket(socket_fd, BACKLOG);

    AcceptConnection(socket_fd, city_state_map, state_list);
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
            // directly jump to next iteration to find the valid addressinfo
            iter = iter->ai_next;
            continue;
        }

        break;
    }

    *valid_addr_info = iter;
    
    // deallocate memory of linked list of unchecked addressinfo
    freeaddrinfo(iter);
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
 * @description: associate the socket file descriptor with a local address
 * @param {int} socket_fd
 * @param {addrinfo} *addr_info
 * @return {*}
 */
void BindSocket(int socket_fd, addrinfo *addr_info) {
    int status_code;
    
    status_code = bind(socket_fd, addr_info->ai_addr, addr_info->ai_addrlen);
    if (status_code == BIND_FAILURE) {
        // deallocate the socket file descripter and terminate main process if failed in bind()
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // std::cout << "start bind to: " << addr_info->ai_addr << std::endl;
}

/**
 * @description: prepare to wait for the incoming connection through the socket file descriptor
 * @reference: Section 5.5, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {int} socket_fd
 * @param {int} backlog, the maximum length to which the queue of pending connections for 
 *              ... socket file descriptor is able to grow
 * @return {*}
 */
void ListenOnSocket(int socket_fd, int backlog) {
    int status_code = listen(socket_fd, backlog);
    if (status_code == LISTEN_FAILURE) {
        exit(EXIT_FAILURE);
    }
    
    std::cout << "Main server is up and running." << std::endl;
}

/**
 * @description: get port number of remote client through the socket file descriptor
 * @param {int} socket_fd
 * @return {int} port number if succeed or -1 if failed
 */
int GetClientPortNumber(int socket_fd) {
    sockaddr_in socket_addr_in;
    socklen_t len = sizeof(socket_addr_in);
    // retrieve the address of the peer(client) connected to the socket_fd
    if (getpeername(socket_fd, (sockaddr *)&socket_addr_in, &len) != -1) {
        // convert the unsigned shor int from network byte order to host byte order
        int port_num = ntohs(socket_addr_in.sin_port);
        return port_num;
    }
    return -1;
}

/**
 * @description: accept the connection request from a clinet in the pending quque and create a
 *              ... child process for each incoming connections to start communication procedure
 * @reference: Section 5.6, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {int} socket_fd
 * @param {map<std::string, std::string> city_state_map}
 * @param {std::string} state_list
 * @return {*}
 */
void AcceptConnection(
    int socket_fd, 
    std::map<std::string, std::string> &city_state_map, 
    std::string &state_list
) {
    sockaddr_storage client_addr;
    socklen_t addr_length;

    std::string city_name;
    std::string response_content;

    while (true) {
        addr_length = sizeof(client_addr);
        int child_socket_fd = accept(socket_fd, (sockaddr*)&client_addr, &addr_length);

        if (child_socket_fd == ACCEPT_FAILURE) {
            std::cout << "Accept Failure" << std::endl;
        }

        GetClientPortNumber(child_socket_fd);

        // notice that father and child process do not share global params with each others
        global_client_id++;
        if (!fork()) {
            int current_client_id = global_client_id;
            // close main socket file descriptor in child process
            close(socket_fd);
            // cyclinicly receive and send contents
            while (true) {
                // use vector to serve as a buffer container instead of char[] to slightly improve
                // ... proformance and make the code more C++ style
                // notice that std::vector should be able to deallocate and clear its usage memory
                // ... itself when programs terminates
                std::vector<char> buffer(4096);
                ReceiveFromClient(child_socket_fd, buffer, city_name, current_client_id);
                
                // send content only if content received from client is not empty
                if (!city_name.empty()) {
                    response_content = QueryStateByCity(city_name, city_state_map, state_list);
                    SendToClient(child_socket_fd, response_content, city_name, current_client_id);
                }
            }
        }
    }
}

/**
 * @description: send contents to specific client via socket file descriptor
 * @param {int} socket_fd
 * @param {string} &content
 * @param {string} city_name
 * @param {int} client_id
 * @return {*}
 */
void SendToClient(int socket_fd, std::string &content, std::string city_name, int client_id) {
    // std::cout << "sending content: " << content << std::endl;
    int status_code;
    status_code = send(socket_fd, content.c_str(), content.size(), 0);

    if (status_code == SEND_FAILURE) {
        // std::cout << "Send Failed" << std::endl;
    }

    // TODO: modify these ugly codes
    if (content == NOT_FOUND_CONTENT) {
        std::cout << "The Main Server has sent "
            << "\""
            << city_name << " "
            << content
            << "\""
            << " to client"
            << client_id
            << " using TCP over port "
            << GetClientPortNumber(socket_fd)
            << std::endl;
    } else {
        std::cout << "Main Server has sent searching result to client"
            << client_id
            << " using TCP over port "
            << GetClientPortNumber(socket_fd)
            << std::endl;
    }

}

/**
 * @description: receive contents from specific client via socket file descriptor
 * @param {int} socket_fd
 * @param {vector<char>} &buffer, receive buffer
 * @param {string} &city_name
 * @param {int} client_id
 * @return {*}
 */
void ReceiveFromClient(int socket_fd, std::vector<char> &buffer, std::string &city_name, int client_id) {
    int recv_length;
    // use vector::data() to get a direct pointer to the continuous memory array used by vector buffer
    // it is equal to &buffer[0]
    recv_length = recv(socket_fd, buffer.data(), buffer.size(), 0);

    // prevent server from receiving empty content, especially when the client is terminated by "Ctrl+C"
    // ... it will send empty content through connection, due to the ugly codes that should be revised
    // TODO: modify these ugly codes 
    if (recv_length == 0) {
        city_name = "";
        return;
    }

    if (recv_length != RECEIVE_FAILURE) {
        // reallocate the buffer to reduce the memory usage
        buffer.resize(recv_length);
        city_name = GetResponseContent(buffer);

        PrintRecvContent(socket_fd, city_name, client_id);
    }
    // close(socket_fd);
    // exit(EXIT_SUCCESS);
}

/**
 * @description: print the contents received in predefined format according to project requirements
 * @param {int} socket_fd
 * @param {string} &city_name
 * @param {int} client_id
 * @return {*}
 */
void PrintRecvContent(int socket_fd, std::string &city_name, int client_id) {
    std::cout << "Mainserver has received the request on city "
        << city_name
        << " from client"
        << client_id
        << " using TCP over port "
        << GetClientPortNumber(socket_fd)
        << std::endl;
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
    
    // std::cout << content << std::endl;

    return content;
}

int main() {

    BootupServer();

    return 0;
}