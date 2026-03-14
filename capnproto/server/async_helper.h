// Copyright (c) 2024 eagle-shop

#ifndef CAPNPROTO_SERVER_ASYNC_HELPER_H_
#define CAPNPROTO_SERVER_ASYNC_HELPER_H_

#include <kj/async.h>
#include <kj/exception.h>

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "cap_constant.h"
#include "log.h"

namespace es_util {
namespace cap {

class AsyncHelper {
 public:
  template <typename DoWorker, typename DoMain>
  static kj::Promise<void> executeAsync(DoWorker&& doWorkerFunc, DoMain&& doMainFunc) noexcept {
    try {
      using T = decltype(doWorkerFunc());
      auto promiseAndCrossThreadFulfiller =
          std::make_shared<kj::PromiseCrossThreadFulfillerPair<T>>(kj::newPromiseAndCrossThreadFulfiller<T>());
      auto executor            = kj::getCurrentThreadExecutor().addRef();
      auto promiseAndFulfiller = kj::newPromiseAndFulfiller<void>();

      if (promiseAndCrossThreadFulfiller && promiseAndCrossThreadFulfiller->fulfiller) {
        std::thread thread([func = std::forward<DoWorker>(doWorkerFunc), promiseAndCrossThreadFulfiller]() {
          try {
            promiseAndCrossThreadFulfiller->fulfiller->fulfill(func());
          } catch (const kj::Exception& e) {
            Log::print(std::string("[AsyncHelper][Async Thread]error kj::Exception: ") + e.getDescription().cStr(),
                       es::LOG_FILE);
            promiseAndCrossThreadFulfiller->fulfiller->reject(
                KJ_EXCEPTION(FAILED, "[AsyncHelper][Async Thread]catch kj::Exception"));
          } catch (const std::exception& e) {
            Log::print(std::string("[AsyncHelper][Async Thread]error std::exception: ") + e.what(), es::LOG_FILE);
            promiseAndCrossThreadFulfiller->fulfiller->reject(
                KJ_EXCEPTION(FAILED, "[AsyncHelper][Async Thread]catch std::exception"));
          } catch (...) {
            Log::print("[AsyncHelper][Async Thread]error unknown exception", es::LOG_FILE);
            promiseAndCrossThreadFulfiller->fulfiller->reject(
                KJ_EXCEPTION(FAILED, "[AsyncHelper][Async Thread]catch unknown exception"));
          }
        });
        return promiseAndCrossThreadFulfiller->promise
            .then([func = std::forward<DoMain>(doMainFunc), thread = std::move(thread)](T&& result) mutable {
              func(std::forward<T>(result));
              thread.join();
            })
            .attach(std::move(promiseAndCrossThreadFulfiller));
      } else {
        Log::print("[AsyncHelper][Async Thread]error promiseAndCrossThreadFulfiller is null", es::LOG_FILE);
        doMainFunc(std::nullopt);
        return kj::READY_NOW;
      }
    } catch (const kj::Exception& e) {
      Log::print(std::string("[AsyncHelper]error kj::Exception: ") + e.getDescription().cStr(), es::LOG_FILE);
      return kj::READY_NOW;
    } catch (const std::exception& e) {
      Log::print(std::string("[AsyncHelper]error std::exception: ") + e.what(), es::LOG_FILE);
      return kj::READY_NOW;
    } catch (...) {
      Log::print("[AsyncHelper]error unknown exception", es::LOG_FILE);
      return kj::READY_NOW;
    }
  }
};

}  // namespace cap
}  // namespace es_util

#endif  // CAPNPROTO_SERVER_ASYNC_HELPER_H_
