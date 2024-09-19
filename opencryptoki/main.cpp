#include <fcntl.h>
#include <opencryptoki/pkcs11.h>

#include <iomanip>
#include <iostream>
#include <memory>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cout << "[debug]Usage: " << argv[0] << " <file_path>" << std::endl;
    return (-1);
  }

  // CK_OBJECT_HANDLE hKey         = CK_INVALID_HANDLE;

  CK_FUNCTION_LIST *pFunctionList = nullptr;
  auto rv                         = C_GetFunctionList(&pFunctionList);
  if (rv != CKR_OK) {
    std::cout << std::hex << "[debug]C_GetFunctionList rv: " << rv << std::endl;
    return (-1);
  }
  if (pFunctionList == nullptr) {
    std::cout << "[debug]pFunctionList is null" << std::endl;
    return (-1);
  }
  if ((pFunctionList->C_Initialize == nullptr) || (pFunctionList->C_GetSlotList == nullptr) ||
      (pFunctionList->C_GetSlotInfo == nullptr) || (pFunctionList->C_OpenSession == nullptr) ||
      (pFunctionList->C_DigestInit == nullptr) || (pFunctionList->C_DigestUpdate == nullptr) ||
      (pFunctionList->C_DigestFinal == nullptr) || (pFunctionList->C_CloseSession == nullptr) ||
      (pFunctionList->C_Finalize == nullptr)) {
    std::cout << "[debug]Any API is null" << std::endl;
    return (-1);
  }

  // Initialize PKCS #11 library and get function list
  CK_C_INITIALIZE_ARGS initArgs = {NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, CKF_OS_LOCKING_OK, NULL_PTR};
  rv                            = pFunctionList->C_Initialize(&initArgs);
  if (rv != CKR_OK) {
    std::cout << std::hex << "[debug]C_Initialize rv: " << rv << std::endl;
    return (-1);
  }

  // Slot
  CK_ULONG slotSize = 0;
  rv                = pFunctionList->C_GetSlotList(CK_FALSE, NULL_PTR, &slotSize);
  if (rv != CKR_OK) {
    std::cout << std::hex << "[debug]C_GetSlotList rv: " << rv << std::endl;
    return (-1);
  }
  std::cout << "[debug]slotSize: " << slotSize << std::endl;
  if (slotSize <= 0) {
    return (-1);
  }

  auto slotIdList = std::make_unique<CK_SLOT_ID[]>(slotSize);
  rv              = pFunctionList->C_GetSlotList(CK_FALSE, slotIdList.get(), &slotSize);
  if (rv != CKR_OK) {
    std::cout << std::hex << "[debug]C_GetSlotList rv: " << rv << std::endl;
    return (-1);
  }

  CK_SLOT_ID availableSlot = 0xFFFFFFFF;
  for (auto i = 0; i < slotSize; i++) {
    CK_SLOT_INFO slotInfo = {};
    rv                    = pFunctionList->C_GetSlotInfo(i, &slotInfo);
    if (rv == CKR_OK) {
      if (slotInfo.flags & CKF_TOKEN_PRESENT) {
        availableSlot = i;
      }
    }
  }
  if (availableSlot == 0xFFFFFFFF) {
    std::cout << "[debug]No slots available" << std::endl;
    return (-1);
  }
  std::cout << "[debug]availableSlot: " << availableSlot << std::endl;

  // Open a session with the token
  CK_SESSION_HANDLE hSession = 0;
  rv = pFunctionList->C_OpenSession(availableSlot, CKF_SERIAL_SESSION, NULL_PTR, NULL_PTR, &hSession);
  if (rv != CKR_OK) {
    std::cout << std::hex << "[debug]C_OpenSession rv: " << rv << std::endl;
    return (-1);
  }

  // Initialize digest operation
  CK_MECHANISM mechanism = {CKM_SHA256, NULL_PTR, 0};
  rv                     = pFunctionList->C_DigestInit(hSession, &mechanism);
  if (rv != CKR_OK) {
    std::cout << std::hex << "[debug]C_DigestInit rv: " << rv << std::endl;
    return (-1);
  }

  // Open the file and read it in chunks
  auto fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    std::cout << std::hex << "[debug]file open error" << rv << std::endl;
    return (-1);
  }

  unsigned char buf[1024] = {};
  ssize_t readSize;
  while ((readSize = read(fd, buf, sizeof(buf))) > 0) {
    rv = pFunctionList->C_DigestUpdate(hSession, buf, readSize);
    std::cout << "[debug]read size: " << readSize << std::endl;
    if (rv != CKR_OK) {
      std::cout << std::hex << "[debug]C_DigestUpdate rv: " << rv << std::endl;
      return (-1);
    }
  }
  close(fd);

  // Finalize digest operation
  CK_BYTE hash[32]  = {};
  CK_ULONG hash_len = sizeof(hash);
  rv                = pFunctionList->C_DigestFinal(hSession, hash, &hash_len);
  if (rv != CKR_OK) {
    std::cout << std::hex << "[debug]C_DigestFinal rv: " << rv << std::endl;
    return (-1);
  }

  // Print hash
  std::cout << "[debug]sha256sum: ";
  for (const auto &e : hash) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)e;
  }
  std::cout << std::dec << std::endl;

  // Close session and finalize PKCS #11 library
  rv = pFunctionList->C_CloseSession(hSession);
  if (rv != CKR_OK) {
    std::cout << std::hex << "[debug]C_CloseSession rv: " << rv << std::endl;
    return (-1);
  }

  rv = pFunctionList->C_Finalize(NULL_PTR);
  if (rv != CKR_OK) {
    std::cout << std::hex << "[debug]C_Finalize rv: " << rv << std::endl;
    return (-1);
  }

  return 0;
}
