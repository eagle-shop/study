#include <gtest/gtest.h>

#include "cap_constant.h"
#include "log.h"

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  es_util::Log::initialize(es::LOG_FILE);
  const auto result = RUN_ALL_TESTS();
  es_util::Log::deinitialize(es::LOG_FILE);

  return result;
}
