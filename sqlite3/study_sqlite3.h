#include <sqlite3.h>

#include <string>

class StudySqlite3 final {
 public:
  explicit StudySqlite3(const std::string& filepath, bool create = false);
  ~StudySqlite3();

  StudySqlite3(const StudySqlite3&)            = delete;
  StudySqlite3(StudySqlite3&&)                 = delete;
  StudySqlite3& operator=(const StudySqlite3&) = delete;
  StudySqlite3& operator=(StudySqlite3&&)      = delete;

  bool integrityCheck();

 private:
  sqlite3* mDb;
};
