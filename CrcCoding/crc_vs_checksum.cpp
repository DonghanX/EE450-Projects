#include <iostream>
#include <string>
#include <functional>
#include <fstream>
#include <cmath>
#include <algorithm>

#include "crc_rx.h"
#include "crc_vs_checksum.h"

// size of each section of checksum calculation

// @brief:  split combo data string into source data string and error bit string
// @params: std::string combo_data, the original data from each row of dataVs.txt file that contains
//          ... both source data and error bit data
// @return: std::pair<std::string, std::string>, pair object with two component string objects
//          ... to store the source data and the error bit data
// @author: Donghan Xia, 2021/09/23
std::pair<std::string, std::string> SplitComboData(std::string combo_data) {
    std::pair<std::string, std::string> data_pair = std::make_pair("", "");

    // find the position of char ' ' in combo data
    int split_position = combo_data.find(0x20);

    if (split_position != combo_data.npos) {
        // implement split action according to the position of char ' '
        data_pair.first = combo_data.substr(0, split_position);
        data_pair.second = combo_data.substr(split_position + 1);
    }

    // std::cout << data_pair.first << std::endl;
    // std::cout << data_pair.second << std::endl;

    return data_pair;
}

// @brief:  apply zeros at the end of source data to meet the needs of CRC-12 mod2 division 
//          ... and checksum calculation
// @params: std::string &source_data, the pass by reference params to provide the address of source_data
//          int diff, the number of zeros needed to applying
// @return: void
// @author: Donghan Xia, 2021/09/19
void ComplementZeros(std::string &source_data, int diff) {
    while (true) {
        // upper-bound condition
        if (diff == 0) {
            break;
        }
        // apply single zero at the end of source data
        source_data.append(1, '0');

        diff--;
    }
}

// @brief:  make division index FindNextDivDigit to next division place
//          same as the XorSingleBit() function defined in crc_tx.cpp
// @params: int &div_index, the pass by reference params to provide the address of division index
//          std::string, the source data that has already been applied with complementary zeros
// @return: int index that points to the start digit of next mod 2 division iteration
// @author: Donghan Xia, 2021/09/19
void FindNextDivDigit(int &div_index, std::string source_data) {
    while (true) {
        // upper-bound condition
        if (div_index >= source_data.size()) {
            break;
        }
        // next non-zero division place condition
        if (source_data[div_index] == '1') {
            break;
        }

        div_index++;
    }
}

// @brief:  bitwise xor operation
//          same as the XorSingleBit() function defined in crc_tx.cpp
// @params: char first_bit
//          char second_bit
// @return: char result of xor operation
// @author: Donghan Xia, 2021/09/19
char XorSingleBit(char first_bit, char second_bit) {
    int result = first_bit xor second_bit;
    // cast int result to char
    return result + '0';
}

// @brief:  introduce bits error to the encoded data
// @params: std::string &encoded_data, the pass by reference params to provide the address of encoded_data
//          std::string error_bit_data, error_bit_data from dataVs.txt
// @return: void
// @author: Donghan Xia, 2021/09/23
void IntroduceErrorBits(std::string &encoded_code, std::string error_bit_data) {
    // use string::find() in a iteration to find all occurence of error bit that refers to '1'
    // position of error bit in
    int error_position = 0;
    char error_char = '1';
    while (true) {
        error_position = error_bit_data.find(error_char, error_position);
        // non-error break condition
        if (error_position == error_bit_data.npos) {
            break;
        }
        // upper-bound break condition
        if (error_position >= error_bit_data.size()) {
            break;
        }
        // std::cout << "error_position: " << error_position << std::endl;

        // perform Xor operation to the encoded data at the current position with '1'
        encoded_code[error_position] = XorSingleBit(encoded_code[error_position], error_char);

        // next find() operation will start from current position + 1
        error_position++;
    }
    // std::cout << "encoded_code with errors: " << encoded_code << std::endl;

}

// @brief:  implement checksum and then check the result with introducing error bits
// @params: std::string combo_data, the original data from each row of dataVs.txt file that contains
//          ... both source data and error bit data
//          std::string generator_data, CRC-12 generator data
// @return: void
// @author: Donghan Xia, 2021/09/23
void ImplementChecksumWithCheck(std::string combo_data) {
    std::string source_data;
    std::string error_bit_data;

    std::string encoded_data;
    std::string checksum_data;

    // split to get source data and error bit data
    std::pair<std::string, std::string> data_pair = SplitComboData(combo_data);
    source_data = data_pair.first;
    error_bit_data = data_pair.second;

    // implement checksum 
    checksum_data = EncodeChecksum(source_data);
    encoded_data = source_data.append(checksum_data);
    // std::cout << "encoded data:" << encoded_data << std::endl;

    // introduce random error bits to the encoded_data;
    IntroduceErrorBits(encoded_data, error_bit_data);

    // check if encoded data should be accepted
    CheckChecksum(encoded_data, checksum_data.size());
}

// @brief:  implement checksum to source data using decimal method
//          also, this function will be reused in receiver part of checksum
// @params: std::string source_data
//          bool needs_print_result, the boolean params that decides whether the result
//          ... should be printed in order to reuse this function without repeatedly 
//          ... printing the checksum result. The default result of this boolean flag is
//          ... true (as declared in the crc_vs_checksum.h)
// @return: std::string, Checksum result data
// @author: Donghan Xia, 2021/09/24
std::string EncodeChecksum(std::string source_data, bool needs_print_result) {
    // complement zeros if the number of digits in source data
    // ... cannot be divided exactly by 8 (number of a single byte)
    int source_byte_remainder = source_data.size() % 8;
    if (source_byte_remainder != 0) {
        ComplementZeros(source_data, source_byte_remainder);
    }

    // split the source data into several byte sections and convert
    // ... each of them to decimal format to calculate the sum
    int split_size = 8;
    int split_index = 0;
    int decimal_sum = 0;
    // store the current section of size 8 data
    std::string byte_data;

    while (true) {
        // upper-bound break condition
        if (split_index >= source_data.size()) {
            break;
        }
        // get current split section
        byte_data = source_data.substr(split_index, split_size);
        // convert byte_data to decimal format
        decimal_sum += ConvertBinaryToDecimal(byte_data);

        split_index += split_size;
    }

    std::string checksum_result = GetBinaryChecksum(decimal_sum, byte_data.size());
    // std::cout << "decimal sum: " << decimal_sum << std::endl;
    if (needs_print_result) {
        std::cout << "checksum: " << checksum_result << "  ";
    }

    return checksum_result;
}

// @brief:  calculate binary checksum data
// @params: int decimal_sum
//          int data_size, binary data size that decides the size of binary result data
// @return: std::string, binary result
// @author: Donghan Xia, 2021/09/25
std::string GetBinaryChecksum(int decimal_sum, int data_size) {
    // wraparound base
    int decimal_base = pow(2, data_size);
    int complementary_base = decimal_base - 1;
    // calculate quotient and remainder of wraparound in decimal method
    int wrap_sum = decimal_sum / decimal_base + decimal_sum % decimal_base;
    // decimal result of checksum
    int decimal_checksum = complementary_base - wrap_sum;

    return ConvertDecimalToBinary(decimal_checksum, data_size);
}

// @brief:  convert decimal to binary by bitwise division with 2
// @params: int decimal_data
//          int data_size, binary data size that decides the size of binary result data
// @return: std::string, binary result
// @author: Donghan Xia, 2021/09/25
std::string ConvertDecimalToBinary(int decimal_data, int data_size) {
    // build the reversed-digit binary data
    int current_index = 0;
    std::string binary_data;
    char current_remainder;
    while (true) {
        // upper-bound break condition
        if (current_index == data_size) {
            break;
        }
        
        current_remainder = decimal_data % 2 + '0';
        decimal_data = decimal_data / 2;
        // build the binary data string in reverse order
        binary_data += current_remainder;

        current_index ++;
    }

    // reverse the binary data string
    std::reverse(binary_data.begin(), binary_data.end());

    return binary_data;

}

// @brief:  convert binary data to decimal by bitwise calculation
// @params: std::string binary_data
// @return: int, decimal result
// @author: Donghan Xia, 2021/09/24
int ConvertBinaryToDecimal(std::string binary_data) {
    // std::cout << "data size: " << binary_data.size() << std::endl;
    int decimal_result = 0;
    int highest_digit = binary_data.size() - 1;
    for (int binary_index = 0; binary_index < binary_data.size(); binary_index++) {
        // std::cout << "exp:" << pow(2, highest_digit - binary_index) << std::endl;
        decimal_result += (binary_data[binary_index] - '0') * pow(2, highest_digit - binary_index);
    }

    return decimal_result;
}

// @brief:  decide if received checksum data should be accepted by checking 
//          ... whether the checksum of source data section is equal to the the checksum data section
// @params: std::string received_data, the encoded data that has implemented checksum
//          int checksum_size, size of checksum data in the received data 
// @return: void
// @author: Donghan Xia, 2021/09/20
void CheckChecksum(std::string received_data, int checksum_size) {
    std::string original_data = received_data;
    // split the received data into two sections: source data and checksum data
    int split_position = received_data.size() - checksum_size;
    std::string source_data = received_data.substr(0, split_position);
    std::string checksum_data = received_data.substr(split_position);

    // std::cout << "source_data: " << source_data << std::endl;
    // std::cout << "checksum_data: " << checksum_data << std::endl;

    // calculate the checksum of the source data section and check if it is equal to the checksum
    // ... data section
    std::string result_word = "pass";
    if (EncodeChecksum(source_data, false) != checksum_data) {
        result_word = "not pass";
    }
    
    std::cout << "result: " << result_word << std::endl << std::endl;
}

// @brief:  decide if received CRC data should be accepted by checking 
//          ... whether its mod 2 division result can be divided exactly (remainder is a zero sequence)
//          same as the CheckCrc() function defined in crc_rx.cpp
// @params: std::string received_data, the encoded data that has implemented CRC
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
//          same as the CheckRemainder() function defined in crc_rx.cpp
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

    std::cout << "result: " << result_words << std::endl;
}

// @brief:  implement CRC to source data
// @params: std::string source_data, original data that are read from dataTx.txt file
//          std::string generator_data, the CRC-12 generator string
// @return: std::string, CRC result data
// @author: Donghan Xia, 2021/09/23
std::string EncodeCrc(std::string source_data, std::string generator_data) {
    // copy source_data to combine with CRC code to get the final codeword
    std::string original_data = source_data;

    // calculate the highest digit bit of CRC generator, which refers to the number of CRC result bits
    int generator_size = generator_data.size();
    int crc_result_size = generator_size - 1;

    // complement zeros
    ComplementZeros(source_data, crc_result_size);

    // calculate size of source data that has been applied zeros
    int source_size = source_data.size();

    // calculate the end-division digit at which the mod 2 division will stop
    int end_div_digit = source_size - generator_size;
    
    // calculate the cut-off digit at witch the applied CRC code starts
    int crc_cutoff_digit = source_size - crc_result_size;
    
    // perform mod 2 division
    int div_index = 0;
    while (true) {
        // upper-bound condition
        if (div_index > end_div_digit) {
            break;
        }

        for (int iter_index = 0; iter_index < generator_size; iter_index++) {
            source_data[div_index + iter_index] 
                    = XorSingleBit(source_data[div_index + iter_index], generator_data[iter_index]);
        }
        // make div_index point to next non-zero bit digit
        FindNextDivDigit(div_index, source_data);
    }

    std::string crc_result_data = source_data.substr(crc_cutoff_digit);
    // std::cout << "codeword: " << std::endl << original_data + crc_result_data << std::endl;
    std::cout << "crc : " << crc_result_data << "  ";
    
    return crc_result_data;
}

// @brief:  implement CRC and then check the result with introducing error bits
// @params: std::string combo_data, the original data from each row of dataVs.txt file that contains
//          ... both source data and error bit data
//          std::string generator_data, CRC-12 generator data
// @return: void
// @author: Donghan Xia, 2021/09/23
void ImplementCrcWithCheck(
    std::string combo_data, 
    std::string generator_data) {

    std::string source_data;
    std::string error_bit_data;

    std::string encoded_data;
    std::string crc_result_data;

    // split to get source data and error bit data
    std::pair<std::string, std::string> data_pair = SplitComboData(combo_data);
    source_data = data_pair.first;
    error_bit_data = data_pair.second;

    // implement CRC
    crc_result_data = EncodeCrc(source_data, generator_data);
    // encoded_data can be obtained by appending CRC code to the original data
    encoded_data = source_data.append(crc_result_data);
    // std::cout << "encoded data:" << encoded_data << std::endl;

    // introduce random error bits to the encoded_data;
    IntroduceErrorBits(encoded_data, error_bit_data);

    // check if encoded data should be accepted
    CheckCrc(encoded_data, generator_data);
}

// @brief:  read all the datas from dataVs.txt via infile stream
// @params: std::function<void(std::string, std::string)>, lambda function that perform actual
//          ... CRC implementation and CRC check
//          std::function<void(std::string)>, lambda function that perform actual checksum
//          ... implementation and check
// @return: void
// @author: Donghan Xia, 2021/09/21
void ReadSourceData(
    std::function<void (std::string, std::string)> const& InvokeCrcFunc, 
    std::function<void (std::string)> const& InvokeChecksumFunc) {
    // define the CRC-12 generator (Hard-Code)
    std::string generator_data = "1100000001111";
    
    // read dataTx.txt file via infile stream
    std::string file_name = "dataVs.txt";
    // combo_data should contains both source data and error bit data
    // ... which can be split by char ' ' (0x20)
    std::string combo_data;

    std::ifstream infile_stream;
    infile_stream.open(file_name.c_str());

    // use ifstream::peek() instead of ifstream::eof() to avoid the redundant end-line problem
    while (infile_stream.peek() != EOF) {
        // obtain data per line in dataTx.txt
        std::getline(infile_stream, combo_data);

        // utilize erase-type lambda function passed by reference value to make function ReadSourceData
        // ... decoupled from actual CRC encode/decode function and actual checksum encode/decode function
        // TODO: replace erase-type lambda function with function template to avoid considerable runtime
        // ... due to repeatedly calling erase-type lambda function
        InvokeCrcFunc(combo_data, generator_data);

        InvokeChecksumFunc(combo_data);
    }

    infile_stream.close();
}

int main() {

    ReadSourceData(ImplementCrcWithCheck, ImplementChecksumWithCheck);

    return 0;
}