// Copyright (c) 2024 eagle-shop

#include "log.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

namespace es_util {

class LogPrivate {
 public:
  static void print(const std::string& str, const std::string& filePath = "");

  static std::mutex mutex;
  static std::unordered_map<std::string, std::ofstream> logFileList;
};

}  // namespace es_util

using namespace es_util;

void LogPrivate::print(const std::string& str, const std::string& filePath) {
  const auto now          = std::chrono::system_clock::now();
  const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
  const auto time         = std::chrono::system_clock::to_time_t(now);
  const auto localTime    = std::localtime(&time);
  if (localTime != nullptr) {
    std::stringstream ss;
    ss << "[" << std::setfill('0') << std::setw(4) << (localTime->tm_year + 1900) << "-" << std::setw(2)
       << (localTime->tm_mon + 1) << "-" << std::setw(2) << localTime->tm_mday << "T" << std::setw(2)
       << localTime->tm_hour << ":" << std::setw(2) << localTime->tm_min << ":" << std::setw(2) << localTime->tm_sec
       << "." << std::setw(6) << microseconds.count() << ((localTime->tm_gmtoff >= 0) ? "+" : "") << std::setw(2)
       << (localTime->tm_gmtoff / 3600) << ":" << std::setw(2) << ((localTime->tm_gmtoff % 3600) / 60)
       << "][thread id: " << std::this_thread::get_id() << "]" << str << std::endl;
    if ((!filePath.empty()) && (LogPrivate::logFileList.count(filePath) != 0)) {
      LogPrivate::logFileList[filePath] << ss.str();
    } else {
      std::cout << ss.str();
    }
  }
}

std::mutex LogPrivate::mutex;
std::unordered_map<std::string, std::ofstream> LogPrivate::logFileList;

bool Log::initialize(const std::string& filePath) {
  const std::lock_guard<std::mutex> lock(LogPrivate::mutex);
  if (LogPrivate::logFileList.count(filePath) != 0) {
    LogPrivate::print("Error: Log file already exists: " + filePath);
    return false;
  }

  std::ofstream ofs(filePath);
  if (!ofs.is_open()) {
    LogPrivate::print("Error: Failed to open log file: " + filePath);
    return false;
  }

  const auto result = LogPrivate::logFileList.emplace(filePath, std::move(ofs));
  if (!result.second) {
    LogPrivate::print("Error: Failed to insert log file into list: " + filePath);
    return false;
  }

  return true;
}

bool Log::deinitialize(const std::string& filePath) {
  const std::lock_guard<std::mutex> lock(LogPrivate::mutex);
  if (LogPrivate::logFileList.count(filePath) == 0) {
    LogPrivate::print("Error: Log file does not exist: " + filePath);
    return false;
  }

  LogPrivate::logFileList.erase(filePath);
  return true;
}

void Log::print(const std::string& str, const std::string& filePath) {
  const std::lock_guard<std::mutex> lock(LogPrivate::mutex);
  LogPrivate::print(str, filePath);
}

void Log::printAndThrow(const std::string& str, const std::string& filePath) {
  print(str, filePath);
  throw(str);
}
