#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "format.h"

using std::string;

string Format::ElapsedTime(long seconds) {
    long days = seconds / (24 * 3600);
    seconds %= (24 * 3600); 
    long hours = seconds / 3600;
    seconds %= 3600;
    long mins = seconds / 60;
    seconds %= 60;
    std::stringstream ss;
    if (days) {
        ss << days << "Days ";
    }
    ss << std::setfill('0') 
       << std::setw(2) << hours << ":" 
       << std::setw(2) << mins << ":" 
       << std::setw(2) << seconds;
    return ss.str();
 }
