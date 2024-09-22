#include <opencryptoki/pkcs11.h>

#include <memory>
#include <string>

class StudyOpencryptoki final {
 public:
  struct bit256 {
    std::string getHashText();
    uint8_t hash[32];
  };

  StudyOpencryptoki();
  ~StudyOpencryptoki();

  StudyOpencryptoki(const StudyOpencryptoki&)            = delete;
  StudyOpencryptoki(StudyOpencryptoki&&)                 = delete;
  StudyOpencryptoki& operator=(const StudyOpencryptoki&) = delete;
  StudyOpencryptoki& operator=(StudyOpencryptoki&&)      = delete;

  std::unique_ptr<bit256> sha256sum(const std::string& filepath);

 private:
  CK_FUNCTION_LIST* mFunctionList;
  CK_SESSION_HANDLE mSessionHandle;
};
