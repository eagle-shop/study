#include "study_sqlite3.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

class StudySqlite3Test : public testing::Test {};

TEST_F(StudySqlite3Test, IntegrityCheck_Ok) {
  StudySqlite3 studySqlite3("./test/data/sqlite3/study.db");
  EXPECT_TRUE(studySqlite3.integrityCheck());
}

TEST_F(StudySqlite3Test, IntegrityCheck_Ng) {
  StudySqlite3 studySqlite3("./test/data/sqlite3/study_broken.db");
  EXPECT_FALSE(studySqlite3.integrityCheck());
}

TEST_F(StudySqlite3Test, EmptyFilePath1) { EXPECT_THROW(StudySqlite3(""), std::string); }

TEST_F(StudySqlite3Test, EmptyFilePath2) { EXPECT_THROW(StudySqlite3("", true), std::string); }

TEST_F(StudySqlite3Test, InvalidFilePath) { EXPECT_THROW(StudySqlite3("./nothing"), std::string); }

TEST_F(StudySqlite3Test, CreateNewDatabase) {
  const std::string newFile("./new_file.db");
  StudySqlite3(newFile, true);
  EXPECT_TRUE(std::filesystem::exists(newFile));
  std::filesystem::remove(newFile);
}
