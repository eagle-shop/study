#include <archive.h>
#include <archive_entry.h>

#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

class Data final {
 public:
  explicit Data(const std::string &fileName, std::size_t heapSizeMB = 50)
      : mFileName(fileName), mHeapSize(1024 * 1024 * heapSizeMB) {}
  ~Data() = default;

  bool openFile() {
    if (mIfs) {
      std::cerr << "File already opened. filename: " << mFileName << std::endl;
      return false;
    }

    mIfs = std::ifstream(mFileName, std::ios_base::in | std::ios_base::binary);
    if (mIfs.value().is_open()) {
      mIfs.value().seekg(0, std::ios_base::beg);
      mBuf = std::make_unique<char[]>(mHeapSize);
      if (!mBuf) {
        std::cerr << "Failed to allocate heap memory. filename: " << mFileName << std::endl;
        closeFile();
        return false;
      }
    } else {
      std::cerr << "File open error. filename: " << mFileName << std::endl;
      closeFile();
      return false;
    }
    return true;
  }

  void closeFile() {
    mIfs.reset();
    mBuf.reset();
  }

  la_ssize_t readFile(const void **buf) {
    try {
      mIfs.value().read(mBuf.get(), mHeapSize);
      *buf = mBuf.get();
      return mIfs.value().gcount();
    } catch (...) {
      std::cerr << "Fatal error. readFile, filename: " << mFileName << std::endl;
      return 0;
    }
  }

  la_ssize_t ignoreFile(std::streamsize size) {
    try {
      mIfs.value().ignore(size);
      return mIfs.value().gcount();
    } catch (...) {
      std::cerr << "Fatal error. ignoreFile, filename: " << mFileName << std::endl;
      return 0;
    }
  }

  la_ssize_t seekFile(std::streamoff off, std::ios_base::seekdir dir) {
    try {
      mIfs.value().seekg(off, dir);
      auto ret = mIfs.value().tellg();
      if (ret < 0) {  // TBD
        std::cout << "[debug]reopen file: " << mFileName << std::endl;
        closeFile();
        openFile();
        mIfs.value().seekg(off, dir);
        ret = mIfs.value().tellg();
      }
      return ret;
    } catch (...) {
      std::cerr << "Fatal error. seekFile, filename: " << mFileName << std::endl;
      return 0;
    }
  }

 private:
  std::optional<std::ifstream> mIfs;
  std::unique_ptr<char[]> mBuf;
  const std::string mFileName;
  const std::size_t mHeapSize;
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
      break;
    } else if (ret == ARCHIVE_RETRY) {
      std::cerr << "[write]ARCHIVE_RETRY" << std::endl;
      break;
    } else if (ret == ARCHIVE_WARN) {
      std::cerr << "[write]ARCHIVE_WARN" << std::endl;
      break;
    } else if (ret == ARCHIVE_FAILED) {
      std::cerr << "[write]ARCHIVE_FAILED" << std::endl;
      break;
    } else if (ret == ARCHIVE_FATAL) {
      std::cerr << "[write]ARCHIVE_FATAL" << std::endl;
      break;
    } else {
      std::cerr << "[write]unknown error" << std::endl;
      ret = ARCHIVE_FATAL;
      break;
    }
  }

  return ret;
}

int main(int argc, char *argv[]) {
  int64_t heapSizeMB             = 0;
  constexpr char errorMessage1[] = "./study_libarchive [heap size (MB)] [output dir (absolute path)] [input.tar_00]...";
  constexpr char errorMessage2[] = "e.g.) ./study_libarchive 50 ./output_dir ./input.tar_00 ./input.tar_01";

  if (argc <= 3) {
    std::cerr << "input error1" << std::endl << errorMessage1 << std::endl << errorMessage2 << std::endl;
    return 0;
  }

  try {
    heapSizeMB = std::stoi(argv[1]);
  } catch (...) {
    std::cerr << "input error2" << std::endl << errorMessage1 << std::endl << errorMessage2 << std::endl;
    return 0;
  }
  if (heapSizeMB <= 0) {
    std::cerr << "input error3" << std::endl << errorMessage1 << std::endl << errorMessage2 << std::endl;
    return 0;
  }

  const std::string outputDir = argv[2];
  std::error_code errorCode;
  if (!std::filesystem::create_directories(outputDir, errorCode) && errorCode) {
    std::cerr << "input error4" << std::endl << errorMessage1 << std::endl << errorMessage2 << std::endl;
    return 0;
  }

  static auto fileOpenCallback = [](struct archive *archiveRead, void *clientData) {
    if (clientData == nullptr) {
      std::cerr << "fileOpenCallback clientData nullptr" << std::endl;
      return ARCHIVE_FATAL;
    }

    return ((Data *)clientData)->openFile() ? ARCHIVE_OK : ARCHIVE_FATAL;
  };

  static auto fileReadCallback = [](struct archive *archiveRead, void *clientData, const void **buffer) {
    if (clientData == nullptr) {
      std::cerr << "fileReadCallback clientData nullptr" << std::endl;
      return la_ssize_t(0);
    }

    return ((Data *)clientData)->readFile(buffer);
  };

  static auto fileSkipCallback = [](struct archive *archiveRead, void *clientData, la_int64_t request) {
    if (clientData == nullptr) {
      std::cerr << "fileSkipCallback clientData nullptr" << std::endl;
      return la_int64_t(-1);
    }

    return ((Data *)clientData)->ignoreFile(request);
  };

  static auto fileSwitchCallback = [](struct archive *archiveRead, void *clientData1, void *clientData2) {
    if (clientData1 != nullptr) {
      ((Data *)clientData1)->closeFile();
    }
    if (clientData2 != nullptr) {
      return fileOpenCallback(archiveRead, clientData2);
    }

    return ARCHIVE_OK;
  };

  static auto fileCloseCallback = [](struct archive *archiveRead, void *clientData) {
    if (clientData == nullptr) {
      std::cerr << "fileCloseCallback clientData nullptr" << std::endl;
      return ARCHIVE_FATAL;
    }

    return fileSwitchCallback(archiveRead, clientData, nullptr);
  };

  static auto fileSeekCallback = [](struct archive *archiveRead, void *clientData, la_int64_t offset, int whence) {
    if (clientData == nullptr) {
      std::cerr << "fileSeekCallback clientData nullptr" << std::endl;
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
        std::cerr << "fileSeekCallback whence error" << std::endl;
        break;
    }

    return ((Data *)clientData)->seekFile(offset, dir);
  };

  auto archiveRead = archive_read_new();
  archive_read_support_filter_all(archiveRead);
  archive_read_support_format_all(archiveRead);

  std::deque<std::unique_ptr<Data>> dataList;
  for (auto i = 3; i < argc; i++) {
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
    std::cerr << "archive_read_open1 error. ret: " << ret << std::endl;
    return 0;
  }

  auto archiveWrite = archive_write_disk_new();
  archive_write_disk_set_options(
      archiveWrite, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);
  archive_write_disk_set_standard_lookup(archiveWrite);

  struct archive_entry *entry = nullptr;
  while (true) {
    ret = archive_read_next_header(archiveRead, &entry);

    if (ret == ARCHIVE_OK) {
      auto entryPathname = archive_entry_pathname(entry);
      if (entryPathname == nullptr) {
        std::cerr << "[read]archive_entry_pathname error" << std::endl;
        break;
      }

      std::string combinPathname = entryPathname;
      if (combinPathname.empty()) {
        std::cerr << "[read]archive_entry_pathname combin error" << std::endl;
        return 0;
      }
      if (combinPathname[0] == '/') {
        // nop
      } else if ((combinPathname.size() >= 2) && (combinPathname[0] == '.') && (combinPathname[1] == '/')) {
        combinPathname.erase(0, 1);
      } else {
        combinPathname.insert(0, "/");
      }

      const auto writePath = outputDir + combinPathname;
      archive_entry_set_pathname(entry, writePath.c_str());
      ret = archive_write_header(archiveWrite, entry);
      if (ret == ARCHIVE_OK) {
        if (archive_entry_size(entry) > 0) {
          copyData(archiveRead, archiveWrite);
        }
      }
    } else if (ret == ARCHIVE_EOF) {
      break;
    } else if (ret == ARCHIVE_RETRY) {
      std::cerr << "[read]ARCHIVE_RETRY" << std::endl;
      break;
    } else if (ret == ARCHIVE_WARN) {
      std::cerr << "[read]ARCHIVE_WARN" << std::endl;
      break;
    } else if (ret == ARCHIVE_FAILED) {
      std::cerr << "[read]ARCHIVE_FAILED" << std::endl;
      break;
    } else if (ret == ARCHIVE_FATAL) {
      std::cerr << "[read]ARCHIVE_FATAL" << std::endl;
      break;
    } else {
      std::cerr << "[read]unknown error" << std::endl;
      break;
    }
  }

  archive_read_close(archiveRead);
  archive_read_free(archiveRead);
  archive_write_close(archiveWrite);
  archive_write_free(archiveWrite);

  return 0;
}
