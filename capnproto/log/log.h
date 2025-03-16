// Copyright (c) 2024 eagle-shop

#ifndef CAPNPROTO_LOG_LOG_H_
#define CAPNPROTO_LOG_LOG_H_

#include <mutex>
#include <string>

class Log final {
 public:
  static void print(const std::string& str);
  static void printAndThrow(const std::string& str);

 private:
  static std::mutex mMutex;
};

#endif  // CAPNPROTO_LOG_LOG_H_
