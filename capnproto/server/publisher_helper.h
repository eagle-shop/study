// Copyright (c) 2024 eagle-shop

#ifndef CAPNPROTO_SERVER_PUBLISHER_HELPER_H_
#define CAPNPROTO_SERVER_PUBLISHER_HELPER_H_

#include <capnp/blob.h>
#include <kj/async.h>
#include <kj/exception.h>
#include <kj/memory.h>

#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "cap_constant.h"
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
                                                         std::function<kj::WaitScope&()>&& getWaitScopeFunc,
                                                         const std::string& logName = "null") {
    return std::shared_ptr<PublisherHelper<Result>>(
        new PublisherHelper<Result>(taskSet, std::move(getWaitScopeFunc), logName));
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

    if (mThreadId == std::this_thread::get_id()) {
      if (!mWorkerThread.joinable()) {
        mExecutor = kj::getCurrentThreadExecutor().addRef();
        if (mExecutor) {
          mWorkerThread = std::thread(
              [this, func = std::forward<F>(func)](const std::atomic<bool>& stopFlag) mutable {
                func(stopFlag);
                mExecutor->executeSync([this] {
                  if (mEndPair.fulfiller) {
                    mEndPair.fulfiller->fulfill();
                  }
                });
              },
              std::cref(mStopFlag));
          ret = true;
        }
      } else {
        Log::print("[server]PublisherHelper::setWorker NG (worker already running) (" + mLogName + ")", es::LOG_FILE);
      }
    } else {
      Log::print("[server]PublisherHelper::setWorker NG (thread id mismatch) (" + mLogName + ")", es::LOG_FILE);
    }

    return ret;
  }

  void stopWorker() {
    Log::print("[server]PublisherHelper::stopWorker start (" + mLogName + ")", es::LOG_FILE);
    if (mWorkerThread.joinable()) {
      mStopFlag.store(true);
      Log::print("[server]PublisherHelper::stopWorker stopFlag: true (" + mLogName + ")", es::LOG_FILE);
      if (mTaskCounter > 0) {
        if (mThreadId == std::this_thread::get_id()) {
          Log::print("[server]PublisherHelper::stopWorker mTaskCounter: " + std::to_string(mTaskCounter) + " (" +
                         mLogName + ")",
                     es::LOG_FILE);
          mCleanupPair.promise.wait(mGetWaitScopeFunc());
        } else {
          Log::print("[server]PublisherHelper::stopWorker NG (thread id mismatch) (" + mLogName + ")", es::LOG_FILE);
        }
      }
      Log::print("[server]PublisherHelper::stopWorker wait (" + mLogName + ")", es::LOG_FILE);
      mEndPair.promise.wait(mGetWaitScopeFunc());
      Log::print("[server]PublisherHelper::stopWorker try to join (" + mLogName + ")", es::LOG_FILE);
      mWorkerThread.join();
      Log::print("[server]PublisherHelper::stopWorker end (" + mLogName + ")", es::LOG_FILE);
    } else {
      Log::print("[server]PublisherHelper::stopWorker NG (worker not running) (" + mLogName + ")", es::LOG_FILE);
    }
  }

  kj::Own<EsUtil::Stream::Server> addSubscriber(std::unique_ptr<typename EsUtil::Callback<Result>::Client> client) {
    if (mThreadId != std::this_thread::get_id()) {
      Log::print("[server]PublisherHelper::addSubscriber NG (thread id mismatch) (" + mLogName + ")", es::LOG_FILE);
      return kj::Own<EsUtil::Stream::Server>();
    }

    if (!mClient.emplace(mNextClientId, std::move(client)).second) {
      Log::print("[server]PublisherHelper::addSubscriber NG (" + mLogName + ")", es::LOG_FILE);
      return kj::Own<EsUtil::Stream::Server>();
    }

    Log::print("[server]PublisherHelper::addSubscriber OK (" + mLogName + ")", es::LOG_FILE);
    return kj::heap<Client>(mNextClientId++, this->shared_from_this(), mLogName);
  }

  template <typename T>
  void publish(T&& value) {
    Log::print("[server]PublisherHelper::publish start (" + mLogName + ")", es::LOG_FILE);
    if (!mStopFlag.load() && mExecutor && mExecutor->isLive() && mTaskSet) {
      try {
        Log::print("[server]PublisherHelper::publish try to executeSync (" + mLogName + ")", es::LOG_FILE);
        mExecutor->executeSync([this, value = std::forward<T>(value)]() {
          Log::print("[server]PublisherHelper::publish start executeSync func (" + mLogName + ")", es::LOG_FILE);
          for (auto& e : mClient) {
            auto callback = std::make_unique<decltype(e.second->sendRequest())>(e.second->sendRequest());
            if (!callback) {
              continue;
            }

            callback->setValue(value);
            mTaskCounter++;
            Log::print("[server]PublisherHelper::publish mTaskSet->add (" + mLogName +
                           "), mTaskCounter: " + std::to_string(mTaskCounter),
                       es::LOG_FILE);
            mTaskSet->add(callback->send()
                              .then(
                                  [this]() {
                                    mTaskCounter--;
                                    Log::print("[server]PublisherHelper::publish callback.send() OK (" + mLogName +
                                                   "), mTaskCounter: " + std::to_string(mTaskCounter),
                                               es::LOG_FILE);
                                    if (mStopFlag.load() && (mTaskCounter == 0)) {
                                      if (mCleanupPair.fulfiller) {
                                        mCleanupPair.fulfiller->fulfill();
                                      }
                                    }
                                  },
                                  [this](kj::Exception&& e) {
                                    mTaskCounter--;
                                    Log::print(
                                        std::string("[server]PublisherHelper::publish callback.send() Exception: ") +
                                            e.getDescription().cStr() + " (" + mLogName +
                                            "), mTaskCounter:" + std::to_string(mTaskCounter),
                                        es::LOG_FILE);
                                    if (mStopFlag.load() && (mTaskCounter == 0)) {
                                      if (mCleanupPair.fulfiller) {
                                        mCleanupPair.fulfiller->fulfill();
                                      }
                                    }
                                  })
                              .attach(kj::mv(callback)));
          }
          Log::print("[server]PublisherHelper::publish end executeSync func (" + mLogName + ")", es::LOG_FILE);
        });
        Log::print("[server]PublisherHelper::publish executeSync end (" + mLogName + ")", es::LOG_FILE);
      } catch (const kj::Exception& e) {
        Log::print(std::string("[server]PublisherHelper::publish kj::Exception: ") + e.getDescription().cStr() + " (" +
                       mLogName + ")",
                   es::LOG_FILE);
      } catch (const std::exception& e) {
        Log::print(std::string("[server]PublisherHelper::publish std::exception: ") + e.what() + " (" + mLogName + ")",
                   es::LOG_FILE);
      } catch (...) {
        Log::print("[server]PublisherHelper::publish unknown exception (" + mLogName + ")", es::LOG_FILE);
      }
    } else {
      Log::print("[server]PublisherHelper::publish mExecutor is " + std::string((mExecutor ? "not null" : "null")) +
                     (mExecutor ? (mExecutor->isLive() ? ", live" : ", dead") : "") + ", mTaskSet is " +
                     (mTaskSet ? "not null" : "null") + " (" + mLogName + ")",
                 es::LOG_FILE);
    }
  }

  bool isWorkerRunning() const { return mWorkerThread.joinable(); }

  ~PublisherHelper() noexcept {
    Log::print("[server]PublisherHelper::~PublisherHelper (" + mLogName + ")", es::LOG_FILE);
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
      Log::print("[server]Client::~Client (" + mLogName + ")" + " mClientId: " + std::to_string(mClientId),
                 es::LOG_FILE);
    }

   private:
    const ClientId mClientId;
    const std::shared_ptr<PublisherHelper> mPublisher;
    const std::string mLogName;
  };

  explicit PublisherHelper(const std::shared_ptr<kj::TaskSet>& taskSet,
                           std::function<kj::WaitScope&()>&& getWaitScopeFunc, const std::string& logName)
      : mTaskSet(taskSet),
        mGetWaitScopeFunc(std::move(getWaitScopeFunc)),
        mLogName(logName),
        mThreadId(std::this_thread::get_id()),
        mStopFlag(false),
        mNextClientId(0),
        mTaskCounter(0),
        mCleanupPair(kj::newPromiseAndFulfiller<void>()),
        mEndPair(kj::newPromiseAndFulfiller<void>()) {
    Log::print("[server]PublisherHelper::PublisherHelper (" + mLogName + ")", es::LOG_FILE);
  }

  void disconnection(ClientId clientId) { mClient.erase(clientId); }

  const std::shared_ptr<kj::TaskSet> mTaskSet;
  const std::function<kj::WaitScope&()> mGetWaitScopeFunc;
  const std::string mLogName;
  const std::thread::id mThreadId;
  kj::Own<const kj::Executor> mExecutor;
  std::thread mWorkerThread;
  std::atomic<bool> mStopFlag;
  ClientId mNextClientId;
  std::unordered_map<std::size_t, std::unique_ptr<typename EsUtil::Callback<Result>::Client>> mClient;
  uint64_t mTaskCounter;
  kj::PromiseFulfillerPair<void> mCleanupPair;
  kj::PromiseFulfillerPair<void> mEndPair;
};

}  // namespace cap
}  // namespace es_util

#endif  // CAPNPROTO_SERVER_PUBLISHER_HELPER_H_
