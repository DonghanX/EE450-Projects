#include <iostream>

void ReadSourceData(
    std::function<void (std::string, std::string)> const& InvokeCrcFunc, 
    std::function<void (std::string)> const& InvokeChecksumFunc
);

std::string EncodeCrc(std::string source_data, std::string generator_data);

void FindNextDivDigit(int &div_index, std::string source_data);

void ImplementCrcWithCheck(std::string source_data, std::string generator_data);

void ImplementChecksumWithCheck(std::string source_data);

char XorSingleBit(char first_bit, char second_bit);

void ComplementZeros(std::string &source_data, int diff);

void IntroduceErrorBits(std::string &source_data, std::string bit_error_data);

void CheckCrc(std::string received_data, std::string generator_data);

void CheckRemainder(std::string result_data);

std::string EncodeChecksum(std::string source_data, bool needs_print_result = true);

void CheckChecksum(std::string received_data, int checksum_size);

int ConvertBinaryToDecimal(std::string binary_data);

std::string ConvertDecimalToBinary(int decimal_data, int data_size);

std::string GetBinaryChecksum(int decimal_sum, int base);

std::pair<std::string, std::string> SplitComboData(std::string combo_data);
