#include <unistd.h>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "process.h"
#include "linux_parser.h"

using std::string;
using std::to_string;
using std::vector;

Process::Process(int id) : pid_(id) { }

int Process::Pid() { 
    return pid_; 
}

float Process::CpuUtilization() const { 
    return 100 * LinuxParser::ActiveJiffies(pid_) / (float) LinuxParser::Jiffies(); 
}

string Process::Command() { 
    return LinuxParser::Command(pid_); 
}

string Process::Ram() const { 
    return LinuxParser::Ram(pid_);
}

string Process::User() { 
    return LinuxParser::User(pid_); 
}

long int Process::UpTime() {
    return LinuxParser::UpTime(pid_); 
}

bool Process::operator<(Process const& a) const { 
    //return stol(this->Ram()) < stol(a.Ram()); 
    return this->CpuUtilization() < a.CpuUtilization();
}