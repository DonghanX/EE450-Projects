#include <iostream>

void TransmitData();

void EncodeCrc(std::string source_data, std::string generator_data);

void FindNextDivDigit(int &div_index, std::string source_data);

void ReadSourceData(std::function<void (std::string, std::string)> const& InvokeFunc);

char XorSingleBit(char first_bit, char second_bit);

void ComplementZeros(std::string &source_data, int diff);