// Copyright (c) 2024 eagle-shop

#include <capnp/blob.h>
#include <capnp/ez-rpc.h>

#include <string>

#include "log.h"
#include "study.capnp.h"

class CallbackX final : public Study::Callback<capnp::Text>::Server {
 public:
  explicit CallbackX(kj::Own<kj::PromiseFulfiller<void>> promiseFulfiller, uint64_t end = 3)
      : mPromiseFulfiller(kj::mv(promiseFulfiller)), mInitialValue(end), mCounter(end) {}
  virtual ~CallbackX() = default;

 private:
  kj::Promise<void> send(SendContext context) final {
    if (context.getParams().hasValue() && mPromiseFulfiller) {
      Log::print(std::string("[client]receive: ") + context.getParams().getValue().cStr() + " " +
                 std::to_string(mCounter) + "/" + std::to_string(mInitialValue));
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

class CallbackY final : public Study::Callback<::Study::Result<::capnp::Text, ::Study::ErrorMessage>>::Server {
 public:
  explicit CallbackY(kj::Own<kj::PromiseFulfiller<void>> promiseFulfiller, uint64_t end = 3)
      : mPromiseFulfiller(kj::mv(promiseFulfiller)), mInitialValue(end), mCounter(end) {}
  virtual ~CallbackY() = default;

 private:
  kj::Promise<void> send(SendContext context) final {
    if (context.getParams().hasValue() && context.getParams().getValue().hasValue() && mPromiseFulfiller) {
      Log::print(std::string("[client]receive: ") + context.getParams().getValue().getValue().cStr() + " " +
                 std::to_string(mCounter) + "/" + std::to_string(mInitialValue));
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

void clientMain() {
  Log::print("[client]start");
  const std::string sock(Study::SOCK.get().cStr());
  const std::string unixSock = "unix:" + sock;

  capnp::EzRpcClient client(unixSock.c_str());
  auto study = client.getMain<Study>();

  auto fetch       = study.fetchRequest();
  auto fetchResult = fetch.send().wait(client.getWaitScope());
  Log::print(std::string("[client]fetchResult: ") + fetchResult.toString().flatten().cStr());

  auto promiseAndFulfiller = kj::newPromiseAndFulfiller<void>();
  auto subscribeX          = study.subscribeXRequest();
  subscribeX.setCallback(kj::heap<CallbackX>(kj::mv(promiseAndFulfiller.fulfiller), 5));
  auto setCallbackResult = subscribeX.send().wait(client.getWaitScope());
  Log::print(std::string("[client]setCallbackResult: ") + setCallbackResult.toString().flatten().cStr());

  {
    auto promiseAndFulfiller2 = kj::newPromiseAndFulfiller<void>();
    auto subscribeX2          = study.subscribeXRequest();
    subscribeX2.setCallback(kj::heap<CallbackX>(kj::mv(promiseAndFulfiller2.fulfiller)));
    auto setCallbackResult2 = subscribeX2.send().wait(client.getWaitScope());
    Log::print(std::string("[client]setCallbackResult2: ") + setCallbackResult2.toString().flatten().cStr());

    promiseAndFulfiller2.promise.wait(client.getWaitScope());
  }

  promiseAndFulfiller.promise.wait(client.getWaitScope());

  promiseAndFulfiller = kj::newPromiseAndFulfiller<void>();
  auto subscribeY     = study.subscribeYRequest();
  subscribeY.setCallback(kj::heap<CallbackY>(kj::mv(promiseAndFulfiller.fulfiller), 5));
  auto setCallbackResultY = subscribeY.send().wait(client.getWaitScope());
  Log::print(std::string("[client]setCallbackResult: ") + setCallbackResultY.toString().flatten().cStr());
  promiseAndFulfiller.promise.wait(client.getWaitScope());

  Log::print("[client]end");
}
