#include <sqlite3.h>

#include <iostream>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cout << "input error" << std::endl;
    return (-1);
  }

  sqlite3 *db = nullptr;
  auto ret    = sqlite3_open(argv[1], &db);
  if ((ret != SQLITE_OK) || (db == nullptr)) {
    std::cout << "sqlite3_open ret: " << ret << std::endl;
    return (-1);
  }

  ret = sqlite3_exec(db, "PRAGMA integrity_check", nullptr, nullptr, nullptr);
  if ((ret != SQLITE_OK) || (db == nullptr)) {
    std::cout << "sqlite3_exec ret: " << ret << std::endl;
    sqlite3_close(db);
    return (-1);
  }

  sqlite3_close(db);

  return 0;
}
