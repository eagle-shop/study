#include <archive.h>
#include <archive_entry.h>

#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

class Data final {
 public:
  explicit Data(const std::string &fileName) : mFileName(fileName) {}
  ~Data() = default;

  bool openFile() {
    if (mIfs) {
      std::cout << "File already opened. filename: " << mFileName << std::endl;
      return false;
    }

    mIfs = std::ifstream(mFileName, std::ios_base::in | std::ios_base::binary);
    if (mIfs.value().is_open()) {
      std::cout << "File open ok. filename: " << mFileName << std::endl;
      mIfs.value().seekg(0, std::ios_base::beg);
      mBuf = std::make_unique<char[]>(mBufSize);
      if (!mBuf) {
        std::cout << "Failed to allocate heap memory. filename: " << mFileName << std::endl;
        closeFile();
        return false;
      }
    } else {
      std::cout << "File open error. filename: " << mFileName << std::endl;
      closeFile();
      return false;
    }
    return true;
  }

  void closeFile() {
    std::cout << "File close. filename: " << mFileName << std::endl;
    mIfs.reset();
    mBuf.reset();
  }

  la_ssize_t readFile(const void **buf) {
    if (!mIfs) {
      std::cout << "Fatal error. readFile, filename: " << mFileName << std::endl;
      return 0;
    }

    mIfs.value().read(mBuf.get(), mBufSize);
    *buf = mBuf.get();
    std::cout << "readSize: " << mIfs.value().gcount() << std::endl;
    return mIfs.value().gcount();
  }

  la_ssize_t ignoreFile(std::streamsize size) {
    if (!mIfs) {
      std::cout << "Fatal error. ignoreFile, filename: " << mFileName << std::endl;
      return 0;
    }

    mIfs.value().ignore(size);
    std::cout << "ignoreSize: " << mIfs.value().gcount() << std::endl;
    return mIfs.value().gcount();
  }

  la_ssize_t seekFile(std::streamoff off, std::ios_base::seekdir dir) {
    if (!mIfs) {
      std::cout << "Fatal error. seekFile, filename: " << mFileName << std::endl;
      return 0;
    }

    mIfs.value().seekg(off, dir);
    std::cout << "seekSize: " << mIfs.value().gcount() << std::endl;
    return mIfs.value().gcount();
  }

 private:
  std::optional<std::ifstream> mIfs;
  std::unique_ptr<char[]> mBuf;
  const std::string mFileName;

  static constexpr std::size_t mBufSize = 1024 * 1024;
};

int copyData(struct archive *archiveRead, struct archive *archiveWrite) {
  int ret           = ARCHIVE_FATAL;
  const void *buf   = nullptr;
  std::size_t size  = 0;
  la_int64_t offset = 0;

  while (true) {
    ret = archive_read_data_block(archiveRead, &buf, &size, &offset);
    if (ret == ARCHIVE_OK) {
      ret = archive_write_data_block(archiveWrite, buf, size, offset);
      if (ret != ARCHIVE_OK) {
        break;
      }
    } else if (ret == ARCHIVE_EOF) {
      std::cout << "[write]ARCHIVE_EOF" << std::endl;
      break;
    } else if (ret == ARCHIVE_RETRY) {
      std::cout << "[write]ARCHIVE_RETRY" << std::endl;
      break;
    } else if (ret == ARCHIVE_WARN) {
      std::cout << "[write]ARCHIVE_WARN" << std::endl;
      break;
    } else if (ret == ARCHIVE_FAILED) {
      std::cout << "[write]ARCHIVE_FAILED" << std::endl;
      break;
    } else if (ret == ARCHIVE_FATAL) {
      std::cout << "[write]ARCHIVE_FATAL" << std::endl;
      break;
    } else {
      std::cout << "[write]unknown error" << std::endl;
      ret = ARCHIVE_FATAL;
      break;
    }
  }

  return ret;
}

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    std::cout << "input error" << std::endl;
    return 0;
  }

  static auto fileOpenCallback = [](struct archive *archiveRead, void *clientData) {
    std::cout << "fileOpenCallback" << std::endl;
    if (clientData == nullptr) {
      std::cout << "fileOpenCallback clientData nullptr" << std::endl;
      return ARCHIVE_FATAL;
    }

    return ((Data *)clientData)->openFile() ? ARCHIVE_OK : ARCHIVE_FATAL;
  };

  static auto fileReadCallback = [](struct archive *archiveRead, void *clientData, const void **buffer) {
    std::cout << "fileReadCallback" << std::endl;
    if (clientData == nullptr) {
      std::cout << "fileReadCallback clientData nullptr" << std::endl;
      return la_ssize_t(0);
    }

    return ((Data *)clientData)->readFile(buffer);
  };

  static auto fileSkipCallback = [](struct archive *archiveRead, void *clientData, la_int64_t request) {
    std::cout << "fileSkipCallback" << std::endl;
    if (clientData == nullptr) {
      std::cout << "fileSkipCallback clientData nullptr" << std::endl;
      return la_int64_t(-1);
    }

    return ((Data *)clientData)->ignoreFile(request);
  };

  static auto fileSwitchCallback = [](struct archive *archiveRead, void *clientData1, void *clientData2) {
    std::cout << "fileSwitchCallback" << std::endl;
    if (clientData1 != nullptr) {
      ((Data *)clientData1)->closeFile();
    }
    if (clientData2 != nullptr) {
      return fileOpenCallback(archiveRead, clientData2);
    }

    return ARCHIVE_OK;
  };

  static auto fileCloseCallback = [](struct archive *archiveRead, void *clientData) {
    std::cout << "fileCloseCallback" << std::endl;
    if (clientData == nullptr) {
      std::cout << "fileCloseCallback clientData nullptr" << std::endl;
      return ARCHIVE_FATAL;
    }

    return fileSwitchCallback(archiveRead, clientData, nullptr);
  };

  static auto fileSeekCallback = [](struct archive *archiveRead, void *clientData, la_int64_t offset, int whence) {
    std::cout << "fileSeekCallback" << std::endl;
    if (clientData == nullptr) {
      std::cout << "fileSeekCallback clientData nullptr" << std::endl;
      return la_int64_t(ARCHIVE_FATAL);
    }

    std::ios_base::seekdir dir = std::ios_base::beg;
    switch (whence) {
      case SEEK_SET:
        dir = std::ios_base::beg;
        break;
      case SEEK_CUR:
        dir = std::ios_base::cur;
        break;
      case SEEK_END:
        dir = std::ios_base::end;
        break;
      default:
        std::cout << "fileSeekCallback whence error" << std::endl;
        break;
    }

    // あってる？
    return ((Data *)clientData)->seekFile(offset, dir);
  };

  auto archiveRead = archive_read_new();
  // archive_read_support_format_tar(archiveRead);
  archive_read_support_filter_all(archiveRead);
  archive_read_support_format_all(archiveRead);

  std::deque<std::unique_ptr<Data>> dataList;
  for (auto i = 1; i < argc; i++) {
    dataList.push_back(std::make_unique<Data>(argv[i]));
    archive_read_append_callback_data(archiveRead, dataList.back().get());
  }

  archive_read_set_open_callback(archiveRead, fileOpenCallback);
  archive_read_set_read_callback(archiveRead, fileReadCallback);
  archive_read_set_skip_callback(archiveRead, fileSkipCallback);
  archive_read_set_close_callback(archiveRead, fileCloseCallback);
  archive_read_set_switch_callback(archiveRead, fileSwitchCallback);
  archive_read_set_seek_callback(archiveRead, fileSeekCallback);
  auto ret = archive_read_open1(archiveRead);
  if (ret != ARCHIVE_OK) {
    std::cout << "archive_read_open1 error. ret: " << ret << std::endl;
    return 0;
  }

  auto archiveWrite = archive_write_disk_new();
  archive_write_disk_set_options(
      archiveWrite, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);
  archive_write_disk_set_standard_lookup(archiveWrite);

  std::stringstream output;
  struct archive_entry *entry = nullptr;
  while (true) {
    ret = archive_read_next_header(archiveRead, &entry);
    if (ret == ARCHIVE_OK) {
      output << archive_entry_pathname(entry) << std::endl;
      ret = archive_write_header(archiveWrite, entry);
      if (ret == ARCHIVE_OK) {
        if (archive_entry_size(entry) > 0) {
          copyData(archiveRead, archiveWrite);
        }
      }
    } else if (ret == ARCHIVE_EOF) {
      std::cout << "[read]ARCHIVE_EOF" << std::endl;
      break;
    } else if (ret == ARCHIVE_RETRY) {
      std::cout << "[read]ARCHIVE_RETRY" << std::endl;
      break;
    } else if (ret == ARCHIVE_WARN) {
      std::cout << "[read]ARCHIVE_WARN" << std::endl;
      break;
    } else if (ret == ARCHIVE_FAILED) {
      std::cout << "[read]ARCHIVE_FAILED" << std::endl;
      break;
    } else if (ret == ARCHIVE_FATAL) {
      std::cout << "[read]ARCHIVE_FATAL" << std::endl;
      break;
    } else {
      std::cout << "[read]unknown error" << std::endl;
      break;
    }
  }

  archive_read_close(archiveRead);
  archive_read_free(archiveRead);
  archive_write_close(archiveWrite);
  archive_write_free(archiveWrite);

  std::cout << "=================================================" << std::endl;
  std::cout << output.str();
  std::cout << "=================================================" << std::endl;

  return 0;
}