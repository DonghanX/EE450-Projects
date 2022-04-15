#include <iostream>
#include <string>
#include <functional>
#include <fstream>

#include "crc_rx.h"

// @brief:  decide if received CRC data should be accepted by checking 
//          ... whether its mod 2 division result can be divided exactly (remainder is a zero sequence)
// @params: std::string received_data, the original data that are read from dataRx.txt file
//          std::string generator_data, the CRC-12 generator string
// @return: void
// @author: Donghan Xia, 2021/09/20
void CheckCrc(std::string received_data, std::string generator_data) {
    std::string original_data = received_data;

    // calculate the highest digit bit of CRC generator
    int generator_size = generator_data.size();

    // calculate size of received data
    int received_size = received_data.size();

    // calculate the end-division digit at which the mod 2 division will stop
    int end_div_digit = received_size - generator_size;
    
    // perform mod 2 division
    int div_index = 0;
    while (true) {
        // upper-bound condition
        if (div_index > end_div_digit) {
            break;
        }

        for (int iter_index = 0; iter_index < generator_size; iter_index++) {
            received_data[div_index + iter_index] 
                    = XorSingleBit(received_data[div_index + iter_index], generator_data[iter_index]);
        }
        // make div_index point to next non-zero bit digit
        FindNextDivDigit(div_index, received_data);
    }

    CheckRemainder(received_data);

}

// @brief:  check if the remainder has char '1'
//          if has, the CRC result is "not pass" because the received data cannot be diveded exactly
//          if not, the CRC result is "pass"
// @params: std::string, the received data
// @return: void
// @author: Donghan Xia, 2021/09/20
void CheckRemainder(std::string result_data) {
    std::string result_words = "pass";

    // use string::find() to locate the position of char '1'
    // string::find() will return to a static value which refers to string::npos
    // ... when there is no char '1' in the given string
    if (result_data.find('1') != result_data.npos) {
        result_words = "not pass";
    }

    std::cout << result_words << std::endl;
}

// @brief:  make division index points to next division place
//          same as the FindNextDivDigit() function defined in crc_tx.cpp
// @params: int &div_index, the pass by reference params to provide the address of division index
//          std::string, the received data
// @return: int index that points to the start digit of next mod 2 division iteration
// @author: Donghan Xia, 2021/09/19
void FindNextDivDigit(int &div_index, std::string received_data) {
    while (true) {
        // upper-bound condition
        if (div_index >= received_data.size()) {
            break;
        }
        // next non-zero division place condition
        if (received_data[div_index] == '1') {
            break;
        }

        div_index++;
    }
}

// @brief:  bitwise xor operation 
//          same as the XorSingleBit defined in crc_tx.cpp
// @params: char first_bit
//          char second_bit
// @return: char result of xor operation
// @author: Donghan Xia, 2021/09/19
char XorSingleBit(char first_bit, char second_bit) {
    int result = first_bit xor second_bit;
    // cast int result to char
    return result + '0';
}

// @brief:  read all the received datas from dataRx.txt via infile stream
//          similar to ReadSourceData() function in crc_tx.cpp
// @params: std::function<void(std::string, std::string)>, lambda function that perform actual
//          ... CRC check
// @return: void
// @author: Donghan Xia, 2021/09/19
void ReadReceivedData(std::function<void (std::string, std::string)> const& InvokeFunc) {
    // define the CRC-12 generator (Hard-Code)
    std::string generator_data = "1100000001111";
    
    // read dataRx.txt file via infile stream
    std::string file_name = "dataRx.txt";
    std::string received_data;

    std::ifstream infile_stream;
    infile_stream.open(file_name.c_str());

    // use ifstream::peek() instead of ifstream::eof() to avoid the redundant end-line problem
    while (infile_stream.peek() != EOF) {
        // obtain data per line in dataTx.txt
        std::getline(infile_stream, received_data);

        // utilize erase-type lambda function passed by reference value to make function ReadReceivedData
        // ... decoupled from actual CRC code check function
        // TODO: replace erase-type lambda function with function template to avoid considerable runtime
        // ... due to repeatedly calling erase-type lambda function
        InvokeFunc(received_data, generator_data);
    }

    infile_stream.close();
}


// @brief:  read all the received datas and then check whether the data should be accepted
// @params: void
// @return: void
// @author: Donghan Xia, 2021/09/20
void ReceiveData() {
    ReadReceivedData(CheckCrc);
}

int main() {

    ReceiveData();

    return 0;
}