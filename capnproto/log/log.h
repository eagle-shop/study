// Copyright (c) 2024 eagle-shop

#ifndef CAPNPROTO_LOG_LOG_H_
#define CAPNPROTO_LOG_LOG_H_

#include <string>

namespace es_util {

class Log final {
 public:
  static bool initialize(const std::string& filePath);
  static bool deinitialize(const std::string& filePath);
  static void print(const std::string& str, const std::string& filePath = "");
  static void printAndThrow(const std::string& str, const std::string& filePath = "");
};

}  // namespace es_util

#endif  // CAPNPROTO_LOG_LOG_H_
