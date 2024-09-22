#include "study_opencryptoki.h"

#include <fcntl.h>

#include <iomanip>
#include <iostream>
#include <memory>

constexpr uint8_t HashLength = 32;

std::string StudyOpencryptoki::bit256::getHashText() {
  std::stringstream ss;
  for (const auto& e : hash) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)e;
  }

  return ss.str();
}

StudyOpencryptoki::StudyOpencryptoki() : mFunctionList(nullptr), mSessionHandle(0xFFFFFFFF) {
  auto ret = C_GetFunctionList(&mFunctionList);
  if (ret != CKR_OK) {
    const std::string errorMessage = std::string("C_GetFunctionList error ret: ") + std::to_string(ret);
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }

  if (mFunctionList == nullptr) {
    const std::string errorMessage("mFunctionList is null");
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }

  if ((mFunctionList->C_Initialize == nullptr) || (mFunctionList->C_GetSlotList == nullptr) ||
      (mFunctionList->C_GetSlotInfo == nullptr) || (mFunctionList->C_OpenSession == nullptr) ||
      (mFunctionList->C_DigestInit == nullptr) || (mFunctionList->C_DigestUpdate == nullptr) ||
      (mFunctionList->C_DigestFinal == nullptr) || (mFunctionList->C_CloseSession == nullptr) ||
      (mFunctionList->C_Finalize == nullptr)) {
    const std::string errorMessage("Any API is null");
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }

  CK_C_INITIALIZE_ARGS initArgs = {NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, CKF_OS_LOCKING_OK, NULL_PTR};
  ret                           = mFunctionList->C_Initialize(&initArgs);
  if (ret != CKR_OK) {
    const std::string errorMessage = std::string("C_Initialize error ret: ") + std::to_string(ret);
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }

  CK_ULONG slotSize = 0;
  ret               = mFunctionList->C_GetSlotList(CK_FALSE, NULL_PTR, &slotSize);
  if ((ret != CKR_OK) || (slotSize <= 0)) {
    const std::string errorMessage =
        std::string("C_GetSlotList error ret: ") + std::to_string(ret) + ", slotSize: " + std::to_string(slotSize);
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }

  auto slotIdList = std::make_unique<CK_SLOT_ID[]>(slotSize);
  ret             = mFunctionList->C_GetSlotList(CK_FALSE, slotIdList.get(), &slotSize);
  if (ret != CKR_OK) {
    const std::string errorMessage = std::string("C_GetSlotList error ret: ") + std::to_string(ret);
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }

  CK_SLOT_ID availableSlot = 0xFFFFFFFF;
  for (auto i = 0; i < slotSize; i++) {
    CK_SLOT_INFO slotInfo = {};
    ret                   = mFunctionList->C_GetSlotInfo(i, &slotInfo);
    if (ret == CKR_OK) {
      if (slotInfo.flags & CKF_TOKEN_PRESENT) {
        availableSlot = i;
      }
    }
  }
  if (availableSlot == 0xFFFFFFFF) {
    const std::string errorMessage("No slots available");
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }

  ret = mFunctionList->C_OpenSession(availableSlot, CKF_SERIAL_SESSION, NULL_PTR, NULL_PTR, &mSessionHandle);
  if (ret != CKR_OK) {
    const std::string errorMessage = std::string("C_OpenSession error ret: ") + std::to_string(ret);
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }
  if (mSessionHandle == 0xFFFFFFFF) {
    const std::string errorMessage("Invalid mSessionHandle");
    std::cerr << errorMessage << std::endl;
    throw(errorMessage);
  }
}

StudyOpencryptoki::~StudyOpencryptoki() {
  auto ret = mFunctionList->C_CloseSession(mSessionHandle);
  if (ret != CKR_OK) {
    std::cerr << "C_CloseSession error ret: " << ret << std::endl;
  }

  ret = mFunctionList->C_Finalize(NULL_PTR);
  if (ret != CKR_OK) {
    std::cerr << "C_Finalize error ret: " << ret << std::endl;
  }
}

std::unique_ptr<StudyOpencryptoki::bit256> StudyOpencryptoki::sha256sum(const std::string& filepath) {
  CK_MECHANISM mechanism = {CKM_SHA256, NULL_PTR, 0};
  auto ret               = mFunctionList->C_DigestInit(mSessionHandle, &mechanism);
  if (ret != CKR_OK) {
    std::cerr << "C_DigestInit error ret: " << ret << std::endl;
    return nullptr;
  }

  auto fd = open(filepath.c_str(), O_RDONLY);
  if (fd < 0) {
    std::cerr << "file open error" << std::endl;
    return nullptr;
  }

  unsigned char buf[1024] = {};
  ssize_t readSize;
  while ((readSize = read(fd, buf, sizeof(buf))) > 0) {
    ret = mFunctionList->C_DigestUpdate(mSessionHandle, buf, readSize);
    if (ret != CKR_OK) {
      std::cerr << "C_DigestUpdate error ret: " << ret << std::endl;
      return nullptr;
    }
  }
  close(fd);

  auto hash = std::make_unique<bit256>();
  if (hash == nullptr) {
    return nullptr;
  }

  CK_ULONG hash_len = HashLength;
  ret               = mFunctionList->C_DigestFinal(mSessionHandle, hash->hash, &hash_len);
  if (ret != CKR_OK) {
    std::cerr << "C_DigestFinal error ret: " << ret << std::endl;
    return nullptr;
  }

  return hash;
}
