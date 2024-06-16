#include <capnp/ez-rpc.h>

#include <iostream>

#include "study.capnp.h"

class Callback final : public Study::Callback::Server {
 public:
  explicit Callback(kj::Own<kj::PromiseFulfiller<void>> promiseFulfiller, uint64_t end = 3)
      : mPromiseFulfiller(kj::mv(promiseFulfiller)), mCounter(end) {}

 private:
  kj::Promise<void> sendText(SendTextContext context) final {
    if (context.getParams().hasText() && mPromiseFulfiller) {
      std::cout << "[client]sendText: " << context.getParams().getText().cStr() << " " << mCounter << std::endl;
      mCounter--;
      if (mCounter == 0) {
        mPromiseFulfiller->fulfill();
        mPromiseFulfiller = nullptr;
      }
    }

    return kj::READY_NOW;
  }

  kj::Own<kj::PromiseFulfiller<void>> mPromiseFulfiller;
  uint64_t mCounter;
};

void clientMain() {
  std::cout << "[client]start" << std::endl;
  const std::string sock(Study::SOCK.get().cStr());
  const std::string unixSock = "unix:" + sock;

  capnp::EzRpcClient client(unixSock.c_str());
  auto study = client.getMain<Study>();

  auto fetchXXX       = study.fetchXXXRequest();
  auto fetchXXXResult = fetchXXX.send().wait(client.getWaitScope());
  std::cout << "[client]fetchXXXResult: " << fetchXXXResult.toString().flatten().cStr() << std::endl;

  auto promiseAndFulfiller = kj::newPromiseAndFulfiller<void>();
  auto subscribeXXX        = study.subscribeXXXRequest();
  subscribeXXX.setCallback(kj::heap<Callback>(kj::mv(promiseAndFulfiller.fulfiller), 5));
  auto setCallbackResult = subscribeXXX.send().wait(client.getWaitScope());
  std::cout << "[client]setCallbackResult: " << setCallbackResult.toString().flatten().cStr() << std::endl;

  {
    auto promiseAndFulfiller2 = kj::newPromiseAndFulfiller<void>();
    auto subscribeXXX2        = study.subscribeXXXRequest();
    subscribeXXX2.setCallback(kj::heap<Callback>(kj::mv(promiseAndFulfiller2.fulfiller)));
    auto setCallbackResult2 = subscribeXXX2.send().wait(client.getWaitScope());
    std::cout << "[client]setCallbackResult2: " << setCallbackResult2.toString().flatten().cStr() << std::endl;

    promiseAndFulfiller2.promise.wait(client.getWaitScope());
  }

  promiseAndFulfiller.promise.wait(client.getWaitScope());

  std::cout << "[client]end" << std::endl;
}
