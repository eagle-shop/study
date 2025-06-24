// Copyright (c) 2024 eagle-shop

#include "log.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

std::mutex Log::mMutex;

void Log::print(const std::string& str) {
  std::lock_guard<std::mutex> lock(mMutex);
  const auto now          = std::chrono::system_clock::now();
  const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
  const auto time         = std::chrono::system_clock::to_time_t(now);
  const auto localTime    = std::localtime(&time);
  if (localTime != nullptr) {
    std::cout << "[" << std::setfill('0') << std::setw(4) << (localTime->tm_year + 1900) << "-" << std::setw(2)
              << (localTime->tm_mon + 1) << "-" << std::setw(2) << localTime->tm_mday << "T" << std::setw(2)
              << localTime->tm_hour << ":" << std::setw(2) << localTime->tm_min << ":" << std::setw(2)
              << localTime->tm_sec << "." << std::setw(6) << microseconds.count()
              << ((localTime->tm_gmtoff >= 0) ? "+" : "") << std::setw(2) << (localTime->tm_gmtoff / 3600) << ":"
              << std::setw(2) << ((localTime->tm_gmtoff % 3600) / 60) << "][thread id: " << std::this_thread::get_id()
              << "]" << str << std::endl;
  }
}

void Log::printAndThrow(const std::string& str) {
  print(str);
  throw(str);
}
