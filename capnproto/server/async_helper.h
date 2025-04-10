// Copyright (c) 2024 eagle-shop

#ifndef CAPNPROTO_SERVER_ASYNC_HELPER_H_
#define CAPNPROTO_SERVER_ASYNC_HELPER_H_

#include <kj/async.h>

#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace es_util {
namespace cap {

class AsyncHelper {
 public:
  template <typename DoWorker, typename DoMain>
  static kj::Promise<void> executeAsync(DoWorker &&doWorkerFunc, DoMain &&doMainFunc) {
    using T = decltype(doWorkerFunc());
    auto promiseAndCrossThreadFulfiller =
        std::make_shared<kj::PromiseCrossThreadFulfillerPair<T>>(kj::newPromiseAndCrossThreadFulfiller<T>());
    if (promiseAndCrossThreadFulfiller) {
      std::thread thread([func = std::forward<DoWorker>(doWorkerFunc), promiseAndCrossThreadFulfiller]() {
        if (promiseAndCrossThreadFulfiller && promiseAndCrossThreadFulfiller->fulfiller) {
          promiseAndCrossThreadFulfiller->fulfiller->fulfill(func());
        }
      });
      return promiseAndCrossThreadFulfiller->promise.then(
          [func = std::forward<DoMain>(doMainFunc), thread = std::move(thread)](T &&result) mutable {
            func(std::forward<T>(result));
            thread.join();
          });
    } else {
      doMainFunc(std::nullopt);
      return kj::NEVER_DONE;
    }
  }
};

};  // namespace cap
};  // namespace es_util

#endif  // CAPNPROTO_SERVER_ASYNC_HELPER_H_
