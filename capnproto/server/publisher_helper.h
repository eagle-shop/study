// Copyright (c) 2024 eagle-shop

#ifndef CAPNPROTO_SERVER_PUBLISHER_HELPER_H_
#define CAPNPROTO_SERVER_PUBLISHER_HELPER_H_

#include <capnp/blob.h>
#include <kj/async.h>
#include <kj/exception.h>
#include <kj/memory.h>

#include <atomic>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "es_util.capnp.h"
#include "log.h"

namespace es_util {
namespace cap {

// Note: All public methods of `PublisherHelper` except `publish()` must be
// called on the same KJ event loop (i.e. the same KJ thread context).
// Those methods are not thread-safe. `publish()` is intended to be
// safe to call from other threads because it uses the stored `mExecutor`
// and `executeSync` to run callbacks on the KJ loop.
template <typename Result>
class PublisherHelper final : public std::enable_shared_from_this<PublisherHelper<Result>> {
 public:
  static std::shared_ptr<PublisherHelper<Result>> create(const std::shared_ptr<kj::TaskSet>& taskSet,
                                                         const std::string& logName = "null") {
    return std::shared_ptr<PublisherHelper<Result>>(new PublisherHelper<Result>(taskSet, logName));
  }

  PublisherHelper(const PublisherHelper&)            = delete;
  PublisherHelper(PublisherHelper&&)                 = delete;
  PublisherHelper& operator=(const PublisherHelper&) = delete;
  PublisherHelper& operator=(PublisherHelper&&)      = delete;

  template <typename F>
  bool setWorker(F&& func) {
    static_assert(std::is_invocable_v<F, const std::atomic<bool>&>,
                  "Worker function must take const std::atomic<bool>& as its first argument");

    bool ret = false;

    if (!mWorkerThread.joinable()) {
      mExecutor     = kj::getCurrentThreadExecutor().addRef();
      mWorkerThread = std::thread(std::forward<F>(func), std::cref(mStopFlag));
      ret           = true;
    }

    return ret;
  }

  kj::Own<EsUtil::Stream::Server> addSubscriber(std::unique_ptr<typename EsUtil::Callback<Result>::Client> client) {
    if (!mClient.emplace(mNextClientId, std::move(client)).second) {
      Log::print("[server]PublisherHelper::addSubscriber NG (" + mLogName + ")");
      return kj::Own<EsUtil::Stream::Server>();
    }

    Log::print("[server]PublisherHelper::addSubscriber OK (" + mLogName + ")");
    return kj::heap<Client>(mNextClientId++, this->shared_from_this(), mLogName);
  }

  template <typename T>
  void publish(T&& value) {
    if (mExecutor && (mExecutor->isLive()) && mTaskSet) {
      try {
        Log::print("[server]PublisherHelper::publish try to executeSync (" + mLogName + ")");
        mExecutor->executeSync([this, value = std::forward<T>(value), &logName = mLogName]() {
          Log::print("[server]PublisherHelper::publish start executeSync func (" + mLogName + ")");
          for (auto& e : mClient) {
            auto callback = std::make_unique<decltype(e.second->sendRequest())>(e.second->sendRequest());
            if (!callback) {
              continue;
            }

            callback->setValue(value);
            mTaskSet->add(callback->send()
                              .then(
                                  [logName]() {
                                    Log::print("[server]PublisherHelper::publish callback.send() OK (" + logName + ")");
                                  },
                                  [logName](kj::Exception&& e) {
                                    Log::print(
                                        std::string("[server]PublisherHelper::publish callback.send() Exception: ") +
                                        e.getDescription().cStr() + " (" + logName + ")");
                                  })
                              .attach(kj::mv(callback)));
          }
        });
        Log::print("[server]PublisherHelper::publish executeSync end (" + mLogName + ")");
      } catch (const kj::Exception& e) {
        Log::print(std::string("[server]PublisherHelper::publish kj::Exception: ") + e.getDescription().cStr() + " (" +
                   mLogName + ")");
      } catch (const std::exception& e) {
        Log::print(std::string("[server]PublisherHelper::publish std::exception: ") + e.what() + " (" + mLogName + ")");
      } catch (...) {
        Log::print("[server]PublisherHelper::publish unknown exception (" + mLogName + ")");
      }
    } else {
      Log::print("[server]PublisherHelper::publish mExecutor is " + std::string((mExecutor ? "not null" : "null")) +
                 (mExecutor ? (mExecutor->isLive() ? ", live" : ", dead") : "") + ", mTaskSet is " +
                 (mTaskSet ? "not null" : "null") + " (" + mLogName + ")");
    }
  }

  bool isWorkerRunning() const { return mWorkerThread.joinable(); }

  ~PublisherHelper() noexcept {
    if (mWorkerThread.joinable()) {
      mStopFlag.store(true);
      Log::print("[server]PublisherHelper::~PublisherHelper try to join (" + mLogName + ")");
      mWorkerThread.join();
      Log::print("[server]PublisherHelper::~PublisherHelper end (" + mLogName + ")");
    }
  }

 private:
  using ClientId = std::size_t;

  class Client final : public EsUtil::Stream::Server {
   public:
    explicit Client(ClientId clientId, const std::shared_ptr<PublisherHelper>& publisher, const std::string& logName)
        : mClientId(clientId), mPublisher(publisher), mLogName(logName) {}
    ~Client() {
      if (mPublisher) {
        mPublisher->disconnection(mClientId);
      }
      Log::print("[server]Client::~Client (" + mLogName + ")" + " mClientId: " + std::to_string(mClientId));
    }

   private:
    const ClientId mClientId;
    const std::shared_ptr<PublisherHelper> mPublisher;
    const std::string mLogName;
  };

  explicit PublisherHelper(const std::shared_ptr<kj::TaskSet>& taskSet, const std::string& logName)
      : mTaskSet(taskSet), mLogName(logName), mStopFlag(false), mNextClientId(0) {}

  void disconnection(ClientId clientId) { mClient.erase(clientId); }

  const std::shared_ptr<kj::TaskSet> mTaskSet;
  const std::string mLogName;
  kj::Own<const kj::Executor> mExecutor;
  std::thread mWorkerThread;
  std::atomic<bool> mStopFlag;
  ClientId mNextClientId;
  std::unordered_map<std::size_t, std::unique_ptr<typename EsUtil::Callback<Result>::Client>> mClient;
};

}  // namespace cap
}  // namespace es_util

#endif  // CAPNPROTO_SERVER_PUBLISHER_HELPER_H_
