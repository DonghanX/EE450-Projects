#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <functional>
#include <algorithm>
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

#include "serverA.h"

// delimiter that splits the city in each even row in list file
#define USER_DELIMITER ','
// delimiter that splits the state in backend's response
#define STATE_DELIMITER ','
// delimiter that splits the state name and user ID in combo data
#define COMBO_DELIMITER '|'
// file name of state-group mapping file
#define LIST_FILE_NAME "dataA.txt"
//
#define SERVER_ID 'A'
// IP address of localhost
#define LOCALHOST "127.0.0.1"
// static port number of localhost
// 451 is the last 3 digits of my USC ID
#define BACKEND_SERVER_PORT "30451"
#define SERVER_MAIN_PORT "32451"
// pre-defined signal for main server to ask backend servers for responsibility list of states
#define RESPONSIBLE_REQUEST_CONTENT "##request##"
// pre-defined User-ID-Not-Found signal
#define NOT_FOUND_CONTENT "###notfound###"


// failue flag
#define SOCKET_FD_FAILURE -1
#define SOCKET_OPTION_FAILURE -1
#define BIND_FAILURE -1
#define RECEIVE_FAILURE -1
#define SEND_FAILURE -1

/**
 * @description: unit test of reading and spliting data file function
 * @param {multimap<std::string, std::vector<std::string>>} state_group_map
 * @return {*}
 */
void TestReadFunction(
    const std::multimap<std::string, std::vector<std::string>> &state_group_map
) {
    std::multimap<std::string, std::vector<std::string>>::const_iterator iter;

    for (iter = state_group_map.begin(); iter != state_group_map.end(); iter++) {
        std::cout << "key: " << iter->first << std::endl;
    }

    std::string state_name;
    std::string user_id;
    std::string result_list;
    std::cout << "input state name: " << std::endl;
    getline(std::cin, state_name);
    std::cout << "input user ID: " << std::endl; 
    getline(std::cin, user_id);

    result_list = GenerateRecommendationUsers(state_name, user_id, state_group_map);
}

/**
 * @description: unit test of recommendation results
 * @param {set<std::string>} result_set
 * @return {*}
 */
void TestRecommendationSet(const std::set<std::string> &result_set) {
    std::set<std::string>::const_iterator iter;
    iter = result_set.begin();
    for (; iter != result_set.end(); iter++) {
        std::cout << *iter << std::endl;
    }
}

void BootupServer() {

    // use multimap to store the state-groups information, where the key is state name and the multiple
    // ... values are social groups information that is stored in the container-set
    std::multimap<std::string, std::vector<std::string>> state_group_map;
    // use vector to store state-only information, which will be uesd in printing all of state
    // ... names if the input city name could not be found
    std::vector<std::string> state_vector;
    
    ReadListInfo(LIST_FILE_NAME, state_group_map, state_vector);

    // TestReadFunction(state_group_map);

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
        ProcessQueryFromMainServer(socket_fd, remote_addr_info, state_group_map);
    }

    // deallocate memory of linked list of unchecked addressinfo
    freeaddrinfo(remote_addr_info);
    freeaddrinfo(local_addr_info);

    close(socket_fd);
}

/**
 * @description: 
 * @param {string} state_name
 * @param {string} user_id
 * @param {multimap<std::string, std::vector<std::string>>} state_group_map
 * @param {string} &result_str
 * @return {*}
 */
std::string GenerateRecommendationUsers(
    std::string state_name, 
    std::string user_id, 
    const std::multimap<std::string, std::vector<std::string>> &state_group_map
) {
    std::string result_list;

    std::multimap<std::string, std::vector<std::string>>::const_iterator iter;
    // get multimap iterator of one or multiple elements
    iter = state_group_map.find(state_name);
    // TODO: utilize auto key word to simplify the declaration of iterator of multimap
    // auto iter = state_group_map.find(state_name);

    std::set<std::string> result_set;

    while (iter != state_group_map.end()) {
        std::vector<std::string> user_vector = iter->second;
        // utilize std::find from <algorithm> to check if the user ID received is in each social group
        auto find_iter = std::find(user_vector.begin(), user_vector.end(), user_id);
        if (find_iter != user_vector.end()) {
            GenerateRecommendationSet(user_id, user_vector, result_set);
        }  
        
        // TODO: add to a set to ensure the recommended users are unique
        iter++;
    }

    result_list = GenerateDelimitedStringFromSet(result_set, USER_DELIMITER);
    PrintRecommendationResult(user_id, state_name, result_list);

    return result_list;
}

/**
 * @description: 
 * @param {string} source_user_id
 * @param {string} state_name
 * @param {string} result_list
 * @return {*}
 */
void PrintRecommendationResult(
    std::string source_user_id, 
    std::string state_name, 
    std::string &result_list
) {
    // check if the result_list is empty
    // an empty result_list string indicates that the user ID cannot be found in the state
    if (result_list.empty()) {
        std::cout << "User " << source_user_id
            << " does not show up in "
            << state_name 
            << std::endl;
    } else {
        std::cout << "Server " << SERVER_ID
            << " found the following possible friends for User "
            << source_user_id
            << " in " << state_name << ": "
            << result_list
            << std::endl;
    }
}

/**
 * @description: 
 * @param {string} source_user_id, source user ID that should not be added to the result set
 * @param {vector<std::string>} &user_vector
 * @param {set<std::string>} &result_set
 * @return {*}
 */
void GenerateRecommendationSet(
    std::string source_user_id, 
    std::vector<std::string> &user_vector, 
    std::set<std::string> &result_set
) {
    // erase source user ID in the original user vector by erase-remove idiom
    auto remove_iter = std::remove(user_vector.begin(), user_vector.end(), source_user_id);
    user_vector.erase(remove_iter);
    
    // convert the vector to set to ensure unique elements
    for (std::string user_id: user_vector) {
        result_set.insert(user_id);
    }
}

/**
 * @description: gather all elements in the set and convert them to a string with a delimiter, 
 * @param {set<std::string>} &result_set
 * @param {char} delimiter
 * @return {*}
 */
std::string GenerateDelimitedStringFromSet(
    const std::set<std::string> &result_set, 
    char delimiter
) {
    std::string result_list;
    std::set<std::string>::iterator iter;
    for (iter = result_set.begin(); iter != result_set.end();) {
        result_list += *iter;
        // notice that the iterator of set is a Biderectional Iterator. We can not simply
        // ... utilize basic operators with the iterator, such as distinct_city_set.end() - 1
        iter++;
        if (iter != result_set.end()) {
            // append delimiter comma pairwisely
            result_list += delimiter;
        }
    }
    return result_list;
}

/**
 * @description: split the user ID list and assign them to the elelemt in vector<string>
 * @param {string} user_list
 * @param {char} delimiter
 * @param {vector<string>} &result_vector
 * @return {*}
 */
void SplitUserList(
    std::string user_list, 
    char delimiter, 
    std::vector<std::string> &result_vector
) {
    int position;

    std::string user_id;
    // avoid recognizing an empty string as a potential user group
    if (user_list.length() == 0) {
        return;
    }

    while (true) {
        // find the position of delimiter
        position = user_list.find(delimiter);

        // upper-bound break condition
        if (position == user_list.npos) {
            // notice that user list may not contain delimiter or the position is correspond to
            // ... the start of the last user in the user list
            // so we simply extract the whole content of the remaining user list, which refers 
            // ... to one certain user ID
            result_vector.push_back(user_list);
            // std::cout << "insert: " << user_list << std::endl;
            break;
        }
        if (position >= user_list.size()) {
            break;
        }

        // extract substring that refers to a certain user ID according to the two delimiters
        user_id = user_list.substr(0, position);
        // remove the user id that has been extracted
        user_list = user_list.substr(position + 1);

        result_vector.push_back(user_id);
        // std::cout << "insert: " << user_id << std::endl;
    }
}

/**
 * @description: receive the query request from main server and replay it with result of finding friends
 * @param {int} socket_fd
 * @param {addrinfo*} remote_addr_info
 * @param {map<string, set<string>>} state_city_map
 * @return {*}
 */
void ProcessQueryFromMainServer(
    int socket_fd, 
    addrinfo* remote_addr_info, 
    std::multimap<std::string, std::vector<std::string>> &state_group_map
) {
    std::string result_list;
    std::string print_msg = "the result(s)";

    std::pair<std::string, std::string> recv_pair;
    std::string combo_data;
    std::string state_name;
    std::string user_id;

    // receive query from main server
    ReceiveFromMainServer(socket_fd, combo_data);

    // split up the received combo data to retrieve the state name and the user ID
    recv_pair = RetrieveStateAndUserIdFromComboData(combo_data, COMBO_DELIMITER);
    state_name = recv_pair.first;
    user_id = recv_pair.second;

    std::cout << "Server " << SERVER_ID
        << " has received a request for finding possible friends of User "
        << user_id
        << " in " << state_name
        << std::endl;

    result_list = GenerateRecommendationUsers(state_name, user_id, state_group_map);

    if (result_list.empty()) {
        print_msg = "\"User " + user_id + " not found\"";
        result_list = NOT_FOUND_CONTENT;
    }

    // send the result list to main server
    SendToMainServer(socket_fd, remote_addr_info, result_list);

    std::cout << "The server " << SERVER_ID
        << " has sent " << print_msg
        << " to Main Server"
        << std::endl;
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

    // std::cout << "state name from combo: " << state_name << std::endl;
    // std::cout << "user id from combo: " << user_id << std::endl;

    return std::make_pair(state_name, user_id);
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

    // check if received content is the pre-defined request signal
    ReceiveFromMainServer(socket_fd, recv_content);
    if (recv_content == RESPONSIBLE_REQUEST_CONTENT) {
        state_list = GenerateLocalResponsibleStateList(state_vector, STATE_DELIMITER);
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
 *              ... that strings up these contents with delimiter
 * @param {vector<string>} state_vector
 * @param {char} delimiter
 * @return {string} state list that contains all state names for which the backend server responsible
 */
std::string GenerateLocalResponsibleStateList(
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
 * @description: read the info file each line and store the state-group mapping information
 * @param {string} file_name, file name of state-group information txt file
 * @param {map<std::string, std::set<string>>} &state_group_map, map that stores state-group elements
 * @param {vector<std::string>} &state_vector, vector that stores state-only elements
 * @return {*}
 */
void ReadListInfo(
    std::string file_name, 
    std::multimap<std::string, std::vector<std::string>> &state_group_map, 
    std::vector<std::string> &state_vector
) {
    std::ifstream infile_stream;

    infile_stream.open(file_name.c_str());
    // assume that each string in odd line is a state name and each string in even line
    // ... contains one or a serie of city names
    // int line_num = 1;
    std::string state_name;
    // a string of cities associated to the same state with delimiter commas
    std::string user_list;

    std::string current_string;
    while (infile_stream.peek() != EOF) {
        getline(infile_stream, current_string);
        // check whether the first letter of current string is alphabetic. If so, the string
        // ... is actually the name of a state. If not, the string is actually the social groups
        if (isalpha(current_string.at(0))) {
            // std::cout << "current line corresponds to state" << std::endl;
            // assign current state name to prepare for the state-group element insertion
            // ... in state-group-map
            state_name = current_string;
            // insert state-only element to the state vector
            state_vector.push_back(state_name);
        } else {
            // insert state-group element
            user_list = current_string;
            std::vector<std::string> current_user_vector;

            SplitUserList(user_list, USER_DELIMITER, current_user_vector);
            InsertStateGroupElement(state_name, current_user_vector, state_group_map);
        }
    }
}

/**
 * @description: insert state-city_set element to the map to store the information
 * @param {string} state_name
 * @param {string} user_list, string that stores IDs of users who are in the same group
 * @param {map<std::string, std::set<string>>} &state_group_map
 * @return {*}
 */
void InsertStateGroupElement(
    std::string state_name, 
    std::vector<std::string> &result_vector, 
    std::multimap<std::string, std::vector<std::string>> &state_group_map
) {
    state_group_map.insert(std::make_pair(state_name, result_vector));
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

