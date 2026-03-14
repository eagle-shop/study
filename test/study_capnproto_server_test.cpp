#include <capnp/ez-rpc.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "cap_constant.h"
#include "log.h"
#include "server.h"
#include "study.capnp.h"

using namespace es;
using namespace es_util;

class StudyCapnpTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { mStudyServer = std::make_unique<StudyServer>(); }
  static void TearDownTestSuite() { mStudyServer.reset(); }

  void SetUp() override {}
  void TearDown() override {}

  static std::unique_ptr<StudyServer> mStudyServer;
};

std::unique_ptr<StudyServer> StudyCapnpTest::mStudyServer;

class CallbackX final : public EsUtil::Callback<capnp::Text>::Server {
 public:
  explicit CallbackX(kj::Own<kj::PromiseFulfiller<void>> promiseFulfiller, uint64_t end = 3)
      : mPromiseFulfiller(kj::mv(promiseFulfiller)), mInitialValue(end), mCounter(end) {}
  ~CallbackX() = default;

 private:
  kj::Promise<void> send(SendContext context) final {
    if (mPromiseFulfiller) {
      Log::print(std::string("[client]receive: ") + context.getParams().toString().flatten().cStr() + " " +
                     std::to_string(mCounter) + "/" + std::to_string(mInitialValue),
                 LOG_FILE);
      mCounter--;
      if (mCounter == 0) {
        mPromiseFulfiller->fulfill();
        mPromiseFulfiller = nullptr;
      }
    }

    return kj::READY_NOW;
  }

  kj::Own<kj::PromiseFulfiller<void>> mPromiseFulfiller;
  const uint64_t mInitialValue;
  uint64_t mCounter;
};

class CallbackY final : public EsUtil::Callback<Result<Study::DailyNotification, Ng>>::Server {
 public:
  explicit CallbackY(kj::Own<kj::PromiseFulfiller<void>> promiseFulfiller, uint64_t end = 3)
      : mPromiseFulfiller(kj::mv(promiseFulfiller)), mInitialValue(end), mCounter(end) {}
  ~CallbackY() = default;

 private:
  kj::Promise<void> send(SendContext context) final {
    if (mPromiseFulfiller) {
      Log::print(std::string("[client]receive: ") + context.getParams().toString().flatten().cStr() + " " +
                     std::to_string(mCounter) + "/" + std::to_string(mInitialValue),
                 LOG_FILE);
      mCounter--;
      if (mCounter == 0) {
        mPromiseFulfiller->fulfill();
        mPromiseFulfiller = nullptr;
      }
    }

    return kj::READY_NOW;
  }

  kj::Own<kj::PromiseFulfiller<void>> mPromiseFulfiller;
  const uint64_t mInitialValue;
  uint64_t mCounter;
};

TEST_F(StudyCapnpTest, wip) {
  Log::print("[client]start", LOG_FILE);
  const std::string sock(Study::SOCK.get().cStr());
  const std::string unixSock = "unix:" + sock;

  capnp::EzRpcClient client(unixSock.c_str());
  auto study = client.getMain<Study>();

  auto createUserId1        = study.createUserIdRequest();
  auto createUserIdPromise1 = createUserId1.send();
  auto createUserId2        = study.createUserIdRequest();
  auto createUserIdPromise2 = createUserId2.send();
  auto createUserIdResult1  = createUserIdPromise1.wait(client.getWaitScope());
  auto createUserIdResult2  = createUserIdPromise2.wait(client.getWaitScope());
  Log::print(std::string("[client]createUserIdResult1: ") + createUserIdResult1.toString().flatten().cStr(), LOG_FILE);
  Log::print(std::string("[client]createUserIdResult2: ") + createUserIdResult2.toString().flatten().cStr(), LOG_FILE);

  auto promiseAndFulfiller = kj::newPromiseAndFulfiller<void>();
  auto subscribeX          = study.subscribeXRequest();
  subscribeX.setCallback(kj::heap<CallbackX>(kj::mv(promiseAndFulfiller.fulfiller), 5));
  auto setCallbackResult = subscribeX.send().wait(client.getWaitScope());
  Log::print(std::string("[client]setCallbackResult: ") + setCallbackResult.toString().flatten().cStr(), LOG_FILE);

  {
    auto promiseAndFulfiller2 = kj::newPromiseAndFulfiller<void>();
    auto subscribeX2          = study.subscribeXRequest();
    subscribeX2.setCallback(kj::heap<CallbackX>(kj::mv(promiseAndFulfiller2.fulfiller)));
    auto setCallbackResult2 = subscribeX2.send().wait(client.getWaitScope());
    Log::print(std::string("[client]setCallbackResult2: ") + setCallbackResult2.toString().flatten().cStr(), LOG_FILE);

    promiseAndFulfiller2.promise.wait(client.getWaitScope());
  }

  promiseAndFulfiller.promise.wait(client.getWaitScope());

  promiseAndFulfiller = kj::newPromiseAndFulfiller<void>();
  auto subscribeY     = study.subscribeYRequest();
  subscribeY.setCallback(kj::heap<CallbackY>(kj::mv(promiseAndFulfiller.fulfiller), 5));
  auto setCallbackResultY = subscribeY.send().wait(client.getWaitScope());
  Log::print(std::string("[client]setCallbackResult: ") + setCallbackResultY.toString().flatten().cStr(), LOG_FILE);
  promiseAndFulfiller.promise.wait(client.getWaitScope());

  if (createUserIdResult1.hasResult() && createUserIdResult1.getResult().hasValue()) {
    auto deleteUserId = study.deleteUserIdRequest();
    deleteUserId.initUserId().setId(createUserIdResult1.getResult().getValue().getId());
    auto deleteUserIdResult = deleteUserId.send().wait(client.getWaitScope());
    Log::print(std::string("[client]deleteUserIdResult: ") + deleteUserIdResult.toString().flatten().cStr(), LOG_FILE);
  }
  if (createUserIdResult2.hasResult() && createUserIdResult2.getResult().hasValue()) {
    auto deleteUserId = study.deleteUserIdRequest();
    deleteUserId.initUserId().setId(createUserIdResult2.getResult().getValue().getId());
    auto deleteUserIdResult = deleteUserId.send().wait(client.getWaitScope());
    Log::print(std::string("[client]deleteUserIdResult: ") + deleteUserIdResult.toString().flatten().cStr(), LOG_FILE);
  }

  Log::print("[client]end", LOG_FILE);
}
