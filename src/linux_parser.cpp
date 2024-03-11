 #include <dirent.h>
#include <unistd.h>
#include <sstream>
#include <vector>
#include <cassert>
#include <boost/filesystem.hpp>
#include <iostream>

#include "linux_parser.h"

using std::stof;
using std::string;
using std::to_string;
using std::vector;

namespace fs = boost::filesystem;

string LinuxParser::OperatingSystem() {
  string line;
  string key;
  string value;
  std::ifstream filestream(kOSPath);
  if (filestream.is_open()) {
    while (std::getline(filestream, line)) {
      std::replace(line.begin(), line.end(), ' ', '_');
      std::replace(line.begin(), line.end(), '=', ' ');
      std::replace(line.begin(), line.end(), '"', ' ');
      std::istringstream linestream(line);
      while (linestream >> key >> value) {
        if (key == "PRETTY_NAME") {
          std::replace(value.begin(), value.end(), '_', ' ');
          return value;
        }
      }
    }
  }
  return value;
}

string LinuxParser::Kernel() {
  string os, kernel, version;
  string line;
  std::ifstream stream(kProcDirectory + kVersionFilename);
  if (stream.is_open()) {
    std::getline(stream, line);
    std::istringstream linestream(line);
    linestream >> os >> version >> kernel;
  }
  return kernel;
}

vector<int> LinuxParser::Pids() {
  vector<int> pids;
  for (const auto& entry : fs::directory_iterator("/proc")) {
    string filename = entry.path().filename().string();
    if (std::all_of(filename.begin(), filename.end(), isdigit)) {
      pids.push_back(stoi(filename));
    }
  }
  return pids;
}

float LinuxParser::MemoryUtilization() { 
  float totalMem, freeMem;
  bool readTotalMem = false;
  bool readFreeMem = false;
  string memType, memVal, memUnit;
  string line;
  std::ifstream stream(kProcDirectory + kMeminfoFilename);
  if (stream.is_open()) {
    while(std::getline(stream, line)) {
      std::istringstream linestream(line);
      linestream >> memType >> memVal >> memUnit;
      if (memType == "MemTotal:") {
        totalMem = stof(memVal);
        readTotalMem = true;
      } else if (memType == "MemFree:") {
        freeMem = stof(memVal);
        readFreeMem = true;
      } else if (readTotalMem && readFreeMem) {
        break;
      }    
    }
  }
  return (totalMem - freeMem) / totalMem; 
}

long LinuxParser::UpTime() { 
  std::ifstream stream(kProcDirectory + kUptimeFilename);
  if (stream.is_open()) {
    string line;
    std::getline(stream, line);
    std::istringstream linestream(line);
    long upTime;
    linestream >> upTime;  
    return upTime;
  }
  return 0;
}

long LinuxParser::Jiffies() { 
  std::ifstream stream(kProcDirectory + kStatFilename);
  string line;
  // Read lines until we find the line starting with "cpu"
  while (std::getline(stream, line)) {
    if (line.substr(0, 3) == "cpu") {
      std::istringstream iss(line);
      std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                      std::istream_iterator<std::string>{}};

      // Sum up all jiffies
      long totalJiffies = 0;
      for (size_t i = 1; i < tokens.size(); ++i) {
        totalJiffies += std::stol(tokens[i]);
      }
      return totalJiffies;
    }
  }
  return 0; // If no CPU line found, return 0
 }

long LinuxParser::ActiveJiffies(int pid) { 
  std::ifstream stream(kProcDirectory + std::to_string(pid) + kStatFilename);
  std::string line;
  if(std::getline(stream, line)) {
    std::istringstream iss(line);
    std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                    std::istream_iterator<std::string>{}};
    // Extract User and Kernel mode CPU times
    assert(tokens.size() >= 15);
    long userJiffies = std::stol(tokens[13]);   // 14th field is user CPU time
    long kernelJiffies = std::stol(tokens[14]); // 15th field is kernel CPU time
    return userJiffies + kernelJiffies;
  }
  return 0;
 }

long LinuxParser::ActiveJiffies() {
  std::ifstream stream(kProcDirectory + kStatFilename);
  string line;
  // Read lines until we find the line starting with "cpu"
  while (std::getline(stream, line)) {
    if (line.substr(0, 3) == "cpu") {
      std::istringstream iss(line);
      std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                      std::istream_iterator<std::string>{}};

      // Sum up all jiffies except idle (tokens[4]) and iowait (tokens[5]) fields
      long totalActiveJiffies = 0;
      for (size_t i = 1; i < tokens.size(); ++i) {
        if (i == 4 || i == 5) {
          continue;
        }
        totalActiveJiffies += std::stol(tokens[i]);
      }
      return totalActiveJiffies;
    }
  }
  return 0; // If no CPU line found, return 0
}

long LinuxParser::IdleJiffies() { 
  std::ifstream stream(kProcDirectory + kStatFilename);
  string line;
  // Read lines until we find the line starting with "cpu"
  while (std::getline(stream, line)) {
    if (line.substr(0, 3) == "cpu") {
      std::istringstream iss(line);
      std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                      std::istream_iterator<std::string>{}};
      return std::stol(tokens[4]); // 5th token represents idle jiffies
    }
  }
  return 0; // If no CPU line found, return 0
 }

float LinuxParser::CpuUtilization() { 
  return ActiveJiffies() / (float) Jiffies();
}

int LinuxParser::TotalProcesses() { 
  return Pids().size(); 
}

int LinuxParser::RunningProcesses() {
  int runningCount = 0;
  // Iterate over the entries in the /proc directory
  for (const auto& entry : fs::directory_iterator("/proc")) {
    if (fs::is_directory(entry)) {
      // Check if the directory name is a number (PID)
      string filename = entry.path().filename().string();
      if (std::all_of(filename.begin(), filename.end(), ::isdigit)) {
        // Read the status file of the process
        std::ifstream statusFile((entry.path() / "status").string());
        string line;
        while (std::getline(statusFile, line)) {
          if (line.substr(0, 6) == "State:") {
            // Check if the process is running (status = "R")
            if (line.find("R (running)") != std::string::npos) {
              runningCount++;
            }
            break;
          }
        }
      }
    }
  }
  return runningCount;
}

string LinuxParser::Command(int pid) { 
  string commandLinePath = kProcDirectory + to_string(pid) + kCmdlineFilename;
  std::ifstream stream(commandLinePath);
  if (!stream.is_open()) {
    std::cerr << "Unable to open " << commandLinePath << "\n";
    return "";
  }
  std::stringstream commandStream;
  char c;
  while (stream.get(c)) {
    if (c == '\0') {
      commandStream << ' ';
    } else {
      commandStream << c;
    }
  }
  return commandStream.str();
}

string LinuxParser::Ram(int pid) {
  string statusFilePath = kProcDirectory + to_string(pid) + kStatusFilename;
  std::ifstream stream(statusFilePath);
  if (!stream.is_open()) {
    std::cerr << "Unable to open " << statusFilePath << "\n";
    return "";
  }
  string line;
  while (std::getline(stream, line)) {
    if (line.find("VmRSS:") != string::npos) {
      std::istringstream iss(line);
      string label;
      long memoryUsage;
      iss >> label >> memoryUsage;
      return to_string(memoryUsage);
    }
  }
  return "";
}

string LinuxParser::Uid(int pid) {
  string statusFilePath = kProcDirectory + to_string(pid) + kStatusFilename;
  std::ifstream stream(statusFilePath);
  if (!stream.is_open()) {
    std::cerr << "Unable to open " << statusFilePath << "\n";
    return "";
  }
  string line;
  while (std::getline(stream, line)) {
    if (line.find("Uid:") != string::npos) {
      std::istringstream iss(line);
      string label;
      int realUid;
      iss >> label >> realUid;
      return to_string(realUid);
    }
  }
  return "";
}

string LinuxParser::User(int pid) {
  string userId = Uid(pid);
  std::ifstream stream(kPasswordPath);
  if (!stream.is_open()) {
    std::cerr << "Unable to open " << kPasswordPath << "\n";
    return "";
  }
  string line;
  while(getline(stream, line)) {
    std::istringstream iss(line);
    string username, _, uid;
    getline(iss, username, ':');
    getline(iss, _, ':'); // skip password
    getline(iss, uid, ':');
    if (uid == userId) {
      return username;
    }
  }
  return "";
}

long LinuxParser::UpTime(int pid) { 
  string statFilePath = kProcDirectory + to_string(pid) + kStatFilename;
  std::ifstream stream(statFilePath);
  if (!stream.is_open()) {
    std::cerr << "Unable to open " << statFilePath << "\n";
    return -1;
  }
  string line;
  std::getline(stream, line);
  std::istringstream iss(line);
  for (int i = 0; i < 21; ++i) {
    iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
  }
  long startTime;
  iss >> startTime;
  return UpTime() - (startTime / sysconf(_SC_CLK_TCK));
 }
