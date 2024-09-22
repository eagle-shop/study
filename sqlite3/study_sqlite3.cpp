#include "study_sqlite3.h"

#include <filesystem>
#include <iostream>

StudySqlite3::StudySqlite3(const std::string &filepath, bool create) : mDb(nullptr) {
  if (filepath.empty() || (!create && !std::filesystem::exists(filepath))) {
    const std::string errorMessage("file not found");
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }

  auto ret = sqlite3_open(filepath.c_str(), &mDb);
  if (ret != SQLITE_OK) {
    const std::string errorMessage = std::string("sqlite3_open ret: ") + std::to_string(ret);
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }

  if (mDb == nullptr) {
    const std::string errorMessage("mDb is null");
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }
}

StudySqlite3::~StudySqlite3() { sqlite3_close(mDb); }

bool StudySqlite3::integrityCheck() {
  auto ret = sqlite3_exec(mDb, "PRAGMA integrity_check", nullptr, nullptr, nullptr);
  if (ret != SQLITE_OK) {
    std::cerr << "sqlite3_exec ret: " << ret << std::endl;
    return false;
  }

  return true;
}
