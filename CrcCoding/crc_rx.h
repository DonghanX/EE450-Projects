#include <iostream>

void CheckCrc(std::string received_data, std::string generator_data);

void CheckRemainder(std::string result_data);

void FindNextDivDigit(int &div_index, std::string received_data);

char XorSingleBit(char first_bit, char second_bit);

void ReceiveData();

void ReadReceivedData(std::function<void (std::string, std::string)> const& InvokeFunc);