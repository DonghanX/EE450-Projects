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

// backlog of queue
#define BACKLOG 5
// delimiter that split the city in each even row in list file
#define CITY_DELIMITER ','
// delimiter that split the state in backend's response
#define STATE_DELIMITER ','
// delimiter that splits the state name and user ID in combo data
#define COMBO_DELIMITER '|'
// IP address of localhost
#define LOCALHOST "127.0.0.1"
// static port number of localhost
// 451 is the last 3 digits of my USC ID
#define SERVER_A_PORT "30451"
#define SERVER_B_PORT "31451"
#define SERVER_MAIN_PORT_UDP "32451"
#define SERVER_MAIN_PORT_TCP "33451"
// pre-defined signal for main server to ask backend servers for responsibility list of states
#define RESPONSIBLE_REQUEST_CONTENT "##request##"
// pre-defined User-ID-Not-Found signal
#define ID_NOT_FOUND_CONTENT "###notfound###"
// pre-defined State-Name-Not-Found signal
#define STATE_NOT_FOUND_CONTENT "###statenotfound###"
// pre-defined backend server id
#define SERVER_A_ID 'A'
#define SERVER_B_ID 'B'

// failue flag
#define SOCKET_FD_FAILURE -1
#define SOCKET_OPTION_FAILURE -1
#define BIND_FAILURE -1
#define LISTEN_FAILURE -1
#define ACCEPT_FAILURE -1
#define RECEIVE_FAILURE -1
#define SEND_FAILURE -1

void BootupServer() {

    std::map<std::string, char> state_backend_map;
    // local addrinfo for TCP
    addrinfo *local_addr_info_tcp;
    // local addrinfo for UDP
    addrinfo *local_addr_info_udp;
    // addrinfo of backend server for UDP
    addrinfo *backend_A_addr_info_udp;
    addrinfo *backend_B_addr_info_udp;

    // socket file descriptor for the socket used in recvfrom() and sendto() for UDP
    int socket_fd_udp;
    // socket file descriptor for TCP
    int socket_fd_tcp;

    RetrieveValidAddrInfo(&local_addr_info_udp, AssembleHints(false), SERVER_MAIN_PORT_UDP, socket_fd_udp, true);
    RetrieveValidAddrInfo(&local_addr_info_tcp, AssembleHints(true), SERVER_MAIN_PORT_TCP, socket_fd_tcp, true);

    // bind local addrinfo to local socket file descriptor for UDP to assign a certain address and 
    // port number to serverMain, which can be utilized in receiving connectionless UDP datagram
    BindSocket(socket_fd_udp, local_addr_info_udp);
    // bind local addrinfo to local socket file descriptor for TCp to assign a certain address and 
    // port number to serverMain, which can be utilized in TCP connection
    BindSocket(socket_fd_tcp, local_addr_info_tcp);

    RetrieveValidAddrInfo(&backend_A_addr_info_udp, AssembleHints(false), SERVER_A_PORT, socket_fd_udp, false);
    RetrieveValidAddrInfo(&backend_B_addr_info_udp, AssembleHints(false), SERVER_B_PORT, socket_fd_udp, false);
    ReusePortIfNeeded(socket_fd_udp);

    std::cout << "Main server is up and running" << std::endl;

    // ask backend server for their responsibilities for corresponding states
    RequestStateListFromBackend(socket_fd_udp, backend_A_addr_info_udp, local_addr_info_udp, 
        state_backend_map, SERVER_A_ID);
    RequestStateListFromBackend(socket_fd_udp, backend_B_addr_info_udp, local_addr_info_udp, 
        state_backend_map, SERVER_B_ID);

    ListStateResponsibility(state_backend_map);

    // start query
    // use pointer array to store all addrinfo pointers of backend servers
    addrinfo* addr_info_array[2];
    addr_info_array[0] = backend_A_addr_info_udp;
    addr_info_array[1] = backend_B_addr_info_udp;

    ListenOnSocket(socket_fd_tcp, BACKLOG);
    AcceptConnection(socket_fd_udp, socket_fd_tcp, addr_info_array, local_addr_info_udp, 
        local_addr_info_tcp, state_backend_map);

    // deallocate memory of linked list of unchecked addressinfo
    freeaddrinfo(backend_A_addr_info_udp);
    freeaddrinfo(backend_B_addr_info_udp);
    freeaddrinfo(local_addr_info_udp);
    freeaddrinfo(local_addr_info_tcp);
    
    close(socket_fd_udp);
    close(socket_fd_tcp);
}

/**
 * @description: prepare to wait for the incoming TCP connection through the socket file descriptor
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
    int socket_fd_udp,
    int socket_fd_tcp, 
    addrinfo **addr_info_array, 
    addrinfo *local_addr_info_udp,
    addrinfo *local_addr_info_tcp, 
    std::map<std::string, char> &state_backend_map
) {
    sockaddr_storage client_addr;
    socklen_t addr_length;

    // incremental client ID that should be visible to changes from all processes
    int client_id = 0;

    while (true) {
        addr_length = sizeof(client_addr);
        int child_socket_fd = accept(socket_fd_tcp, (sockaddr*)&client_addr, &addr_length);
        if (child_socket_fd == ACCEPT_FAILURE) {
            std::cout << "Accept Failure" << std::endl;
        }

        // notice that father and child process do not share params with each others
        client_id++;
        if (!fork()) {
            std::string query_result;
            std::string combo_data;
            std::pair<std::string, std::string> info_pair;
            int current_client_id = client_id;
            // default backend_id is set to 0 to be used in the following check of whether the 
            // ... input state name could be found
            char backend_id = '0';

            // close main socket file descriptor in child process
            close(socket_fd_tcp);
            // cyclinicly receive and send contents
            while (true) {
                std::vector<char> buffer(4096);
                ReceiveFromClient(child_socket_fd, buffer, combo_data, current_client_id);
                info_pair = RetrieveStateAndUserIdFromComboData(combo_data, COMBO_DELIMITER);
                //
                ProcessQuery(socket_fd_udp, addr_info_array, local_addr_info_udp, info_pair, 
                    state_backend_map, query_result, backend_id);

                SendResultToClient(child_socket_fd, local_addr_info_tcp, client_id, backend_id, 
                    info_pair, query_result);
            }
        }
    }
}

/**
 * @description: split string with delimiter and add two substrings to a pair
 * @param {string} combo_data
 * @param {char} delimiter
 * @return {*}
 */
std::pair<std::string, std::string> RetrieveStateAndUserIdFromComboData(
    std::string combo_data, 
    char delimiter
) {
    int position = combo_data.find(delimiter);
    std::string state_name = combo_data.substr(0, position);
    std::string user_id = combo_data.substr(position + 1);

    return std::make_pair(state_name, user_id);
}

/**
 * @description: receive contents from specific client via TCP
 * @param {int} socket_fd
 * @param {vector<char>} &buffer, receive buffer
 * @param {string} &combo_data
 * @param {int} client_id
 * @return {*}
 */
void ReceiveFromClient(int socket_fd, std::vector<char> &buffer, std::string &combo_data, int client_id) {
    int recv_length;
    // use vector::data() to get a direct pointer to the continuous memory array used by vector buffer
    // it is equal to &buffer[0]
    recv_length = recv(socket_fd, buffer.data(), buffer.size(), 0);

    // prevent server from receiving empty content, especially when the client is terminated by "Ctrl+C"
    // ... it will send empty content through connection, due to the ugly codes that should be revised
    // TODO: modify these ugly codes 
    if (recv_length == 0) {
        combo_data = "";
        // close child socket file descriptor and exit current child process to avoid infinite loop of
        // ... query processing
        close(socket_fd);
        exit(EXIT_SUCCESS);
    }

    if (recv_length != RECEIVE_FAILURE) {
        // reallocate the buffer to reduce the memory usage
        buffer.resize(recv_length);
        combo_data = ConvertReceivedContent(buffer);
    }
}

/**
 * @description: pre-define some of the addrinfo parameters
 * @reference: Section 5.1, Beej’s Guide to Network Programming
 *              ... https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf
 * @param {bool} is_tcp
 * @return {addrinfo} addrinfo with pre-defined parameters
 */
addrinfo AssembleHints(bool is_tcp) {
    addrinfo hints;

    // reset addrinfo struct to empty or the return value of getaddrinfo() might 
    // ... takes from some of the EAI_ERROR code
    memset(&hints, 0, sizeof(hints));
    // either ipv4 or ipv6 is ok
    hints.ai_family = AF_UNSPEC;
    if (is_tcp) {
        hints.ai_socktype = SOCK_STREAM;
    } else {
        hints.ai_socktype = SOCK_DGRAM;
    }
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
 * @description: retrieve port number
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
 * @description: check if the input state_name can be found; send state name and user ID to 
 *              ... backend server and receive the query result; receive query result from backend 
 *              ... server
 * @TODO: optimize number of params
 * @param {int} socket_fd_udp
 * @param {addrinfo**} addr_info_array, array that stores all the backend server addrinfo* for UDP
 * @param {addrinfo*} local_addr_info
 * @param {pair<string, string>} &info_pair, pair<state_name, user_id>
 * @param {map<std::string, int>&}
 * @param {char} &backend_id
 * @return {*}
 */
void ProcessQuery(
    int socket_fd_udp,
    addrinfo **addr_info_array, 
    addrinfo *local_addr_info_udp,
    std::pair<std::string, std::string> &info_pair, 
    std::map<std::string, char> &state_backend_map, 
    std::string &query_result, 
    char &backend_id
) {
    // index in backend addrinfo array
    int backend_index;
    int port_num;
    std::string state_name = info_pair.first;
    std::string send_content = info_pair.first + COMBO_DELIMITER + info_pair.second;

    // find which server is responsible for the input state name
    // firstly we check if the input state does not belong to any backend server
    // since all elements in a map are unique, the map::count() can only return 1
    // ... if the key exists
    if (!state_backend_map.count(state_name)) {
        std::cout << state_name << " does not show up in server A&B" << std::endl;
        query_result = STATE_NOT_FOUND_CONTENT;
        return;
    }
    backend_id = state_backend_map[state_name];
    std::cout << state_name << " shows up in server " << backend_id << std::endl;

    backend_index = ConvertBackendIdIntoIndex(backend_id);
    // send the input state name to corresponding backend server
    SendToBackend(socket_fd_udp, addr_info_array[backend_index], send_content);

    port_num = GetLocalPortNumber(local_addr_info_udp);
    std::cout << "The Main Server has sent request for " 
        << state_name
        << " to server " << backend_id
        << " using UDP over port " << port_num
        << std::endl;

    // receive result sent from backend server
    ReceiveFromBackend(socket_fd_udp, query_result);

}

/**
 * @description: send querying result to the client via TCP 
 * @TODO: rebase these ugly codes
 * @param {int} socket_fd
 * @param {addrinfo*} local_addr_info
 * @param {int} client_id
 * @param {char} backend_id
 * @param {pair<string, string>} &info_pair, pair<state_name, user_id>
 * @param {string} &query_result
 * @return {*}
 */
void SendResultToClient(
    int socket_fd, 
    addrinfo *local_addr_info, 
    int client_id, 
    char backend_id, 
    std::pair<std::string, std::string> &info_pair, 
    std::string &query_result
) {
    int status_code;
    bool not_found_flag = false;
    std::string state_name;
    std::string user_id;

    state_name = info_pair.first;
    user_id = info_pair.second;

    std::string print_msg_recv = "searching result of User " + user_id;
    std::string print_msg_send = "searching result(s)";

    // check if the state name could be found firstly
    if (query_result == STATE_NOT_FOUND_CONTENT) {
        print_msg_send = "\"" + state_name + ": Not found\"";
    } else if (query_result == ID_NOT_FOUND_CONTENT) {
    // check if the user ID cannot be found in the backend server
        print_msg_recv = "\"User " + user_id + ": Not found\"";
        not_found_flag = true;
    } else {
        std::cout << "Main server has received " 
            << print_msg_recv 
            << " from server " << backend_id 
            << std::endl;
    }

    status_code = send(socket_fd, query_result.c_str(), query_result.size(), 0);

    if (status_code == SEND_FAILURE) {
        std::cout << "send failure" << std::endl;
    }
    
    if (not_found_flag) {
        print_msg_send = "message";
    }
    std::cout << "Main Server has sent "
        << print_msg_send
        << " to client " << client_id
        << " using TCP over port"
        << GetLocalPortNumber(local_addr_info)
        << std::endl;
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

