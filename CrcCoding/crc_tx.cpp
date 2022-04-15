#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <functional>

#include "crc_tx.h"

// @brief:  implement CRC to source data and show the results in terminal
// @params: std::string source_data, the original data that are read from dataTx.txt file
//          std::string generator_data, the CRC-12 generator string
// @return: void
// @author: Donghan Xia, 2021/09/19
void EncodeCrc(std::string source_data, std::string generator_data) {
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

    // print result in terminal
    std::string crc_result_data = source_data.substr(crc_cutoff_digit);
    std::cout << "codeword: " << std::endl << original_data + crc_result_data << std::endl;
    std::cout << "crc: " << std::endl << crc_result_data << std::endl;
}

// @brief:  apply zeros at the end of source data to meet the needs of CRC-12 mod2 division
// @params: std::string &source_data, he pass by reference params to provide the address of source_data
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

// @brief:  make division index points to next division place
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
// @params: char first_bit
//          char second_bit
// @return: char result of xor operation
// @author: Donghan Xia, 2021/09/19
char XorSingleBit(char first_bit, char second_bit) {
    int result = first_bit xor second_bit;
    // cast int result to char
    return result + '0';
}

// @brief:  read all the transmission datas from dataTx.txt via infile stream
// @params: std::function<void(std::string, std::string)>, lambda function that perform actual
//          ... CRC implementation
// @return: void
// @author: Donghan Xia, 2021/09/19
void ReadSourceData(std::function<void (std::string, std::string)> const& InvokeFunc) {
    // define the CRC-12 generator (Hard-Code)
    std::string generator_data = "1100000001111";
    
    // read dataTx.txt file via infile stream
    std::string file_name = "dataTx.txt";
    std::string source_data;

    std::ifstream infile_stream;
    infile_stream.open(file_name.c_str());

    // use ifstream::peek() instead of ifstream::eof() to avoid the redundant end-line problem
    while (infile_stream.peek() != EOF) {
        // obtain data per line in dataTx.txt
        std::getline(infile_stream, source_data);

        // utilize erase-type lambda function passed by reference value to make function ReadSourceData
        // ... decoupled from actual CRC implementation function
        // TODO: replace erase-type lambda function with function template to avoid considerable runtime
        // ... due to repeatedly calling erase-type lambda function
        InvokeFunc(source_data, generator_data);
    }

    infile_stream.close();
}

// @brief:  read all the transmission datas and then implement CRC to each data code
// @params: void
// @return: void
// @author: Donghan Xia, 2021/09/19
void TransmitData() {
    ReadSourceData(EncodeCrc);
}

int main() {

    TransmitData();

    return 0;
}

