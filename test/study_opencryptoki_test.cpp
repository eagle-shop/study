#include "study_opencryptoki.h"

#include <gtest/gtest.h>

#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

class StudyOpencryptokiTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    std::ifstream ifs("./test/data/opencryptoki/sha256sum.txt");
    if (!ifs.is_open()) {
      std::terminate();
    }

    mCorrectData = std::make_unique<std::unordered_map<std::string, std::string>>();
    if (!mCorrectData) {
      std::terminate();
    }

    std::string line;
    while (std::getline(ifs, line)) {
      mCorrectData->emplace(line.substr(line.find_last_of('/') + 1), line.substr(0, line.find_first_of(' ')));
      line.clear();
    }

    if (mCorrectData->size() <= 0) {
      std::terminate();
    }
  }

  static void TearDownTestSuite() {
    if (mCorrectData) {
      mCorrectData.reset();
    }
  }

  static std::unique_ptr<std::unordered_map<std::string, std::string>> mCorrectData;
};

std::unique_ptr<std::unordered_map<std::string, std::string>> StudyOpencryptokiTest::mCorrectData = nullptr;

TEST_F(StudyOpencryptokiTest, Ok) {
  for (const auto& e : *mCorrectData) {
    const std::string filepath = std::string("./test/data/opencryptoki/") + e.first;
    auto ret                   = StudyOpencryptoki().sha256sum(filepath);
    ASSERT_NE(ret, nullptr);
    EXPECT_EQ(ret->getHashText(), e.second);
  }
}

TEST_F(StudyOpencryptokiTest, NG) { EXPECT_EQ(StudyOpencryptoki().sha256sum("./nothing"), nullptr); }
