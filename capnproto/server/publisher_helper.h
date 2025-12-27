// Copyright (c) 2024 eagle-shop

#ifndef CAPNPROTO_SERVER_PUBLISHER_HELPER_H_
#define CAPNPROTO_SERVER_PUBLISHER_HELPER_H_

#include <capnp/blob.h>
#include <kj/async.h>
#include <kj/exception.h>
#include <kj/memory.h>
#include <kj/mutex.h>

#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "es_util.capnp.h"
#include "log.h"

namespace es_util {
namespace cap {

using ClientId = std::size_t;

class ClientInterface {
 public:
  virtual ~ClientInterface() {}
  virtual void disconnection(ClientId clientId) = 0;
};

class Client final : public EsUtil::Stream::Server {
 public:
  explicit Client(ClientId clientId, const std::shared_ptr<ClientInterface>& interface)
      : mClientId(clientId), mInterface(interface) {}
  virtual ~Client() {
    if (mInterface) {
      mInterface->disconnection(mClientId);
    }
  }

 private:
  const ClientId mClientId;
  const std::shared_ptr<ClientInterface> mInterface;
};

template <typename Result>
class PublisherHelper final : public ClientInterface, public std::enable_shared_from_this<PublisherHelper<Result>> {
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
    static_assert(std::is_invocable_v<F, std::stop_token>,
                  "Worker function must take std::stop_token as its first argument");

    bool ret = false;

    if (!mWorkerThread.joinable()) {
      *mExecutor.lockExclusive() = kj::getCurrentThreadExecutor();
      mWorkerThread              = std::jthread(std::forward<F>(func));
      ret                        = true;
    }

    return ret;
  }

  kj::Own<Client> addSubscriber(std::unique_ptr<typename EsUtil::Callback<Result>::Client> client) {
    if (!mClient.emplace(mClient.size(), std::move(client)).second) {
      Log::print("[server]PublisherHelper::addSubscriber NG (" + mLogName + ")");
      return kj::Own<Client>();
    }

    Log::print("[server]PublisherHelper::addSubscriber OK (" + mLogName + ")");
    return kj::heap<Client>(mClient.size() - 1, this->shared_from_this());
  }

  template <typename T>
  void publish(T&& value) {
    auto lock = mExecutor.lockExclusive();
    KJ_IF_MAYBE (exec, *lock) {
      if ((exec != nullptr) && (exec->isLive()) && mTaskSet) {
        try {
          Log::print("[server]PublisherHelper::publish try to executeSync (" + mLogName + ")");
          exec->executeSync([this, value = std::forward<T>(value), &logName = mLogName]() {
            Log::print("[server]PublisherHelper::publish start executeSync func (" + mLogName + ")");
            for (auto& e : mClient) {
              auto callback = std::make_unique<decltype(e.second->sendRequest())>(e.second->sendRequest());
              if (!callback) {
                continue;
              }

              callback->setValue(value);
              mTaskSet->add(
                  callback->send()
                      .then(
                          [logName]() {
                            Log::print("[server]PublisherHelper::publish callback.send() OK (" + logName + ")");
                          },
                          [logName](kj::Exception&& e) {
                            Log::print(std::string("[server]PublisherHelper::publish callback.send() Exception: ") +
                                       e.getDescription().cStr() + " (" + logName + ")");
                          })
                      .attach(kj::mv(callback)));
            }
          });
          Log::print("[server]PublisherHelper::publish executeSync end (" + mLogName + ")");
        } catch (const kj::Exception& e) {
          Log::print(std::string("[server]PublisherHelper::publish kj::Exception: ") + e.getDescription().cStr() +
                     " (" + mLogName + ")");
        } catch (const std::exception& e) {
          Log::print(std::string("[server]PublisherHelper::publish std::exception: ") + e.what() + " (" + mLogName +
                     ")");
        } catch (...) {
          Log::print("[server]PublisherHelper::publish unknown exception (" + mLogName + ")");
        }
      } else {
        Log::print("[server]PublisherHelper::publish exec is " +
                   std::string(((exec != nullptr) ? "not null" : "null")) +
                   ((exec != nullptr) ? (exec->isLive() ? ", live" : ", dead") : "") + ", mTaskSet is " +
                   (mTaskSet ? "not null" : "null") + " (" + mLogName + ")");
      }
    } else {
      Log::print("[server]PublisherHelper::publish exec is null (" + mLogName + ")");
    }
  }

  operator bool() const { return mWorkerThread.joinable(); }

  virtual ~PublisherHelper() noexcept {
    if (mWorkerThread.joinable()) {
      *mExecutor.lockExclusive() = nullptr;
      mWorkerThread.request_stop();
      Log::print("[server]PublisherHelper::~PublisherHelper try to join (" + mLogName + ")");
      mWorkerThread.join();
      Log::print("[server]PublisherHelper::~PublisherHelper end (" + mLogName + ")");
    }
  }

 private:
  explicit PublisherHelper(const std::shared_ptr<kj::TaskSet>& taskSet, const std::string& logName)
      : mTaskSet(taskSet), mLogName(logName) {}

  void disconnection(ClientId clientId) final { mClient.erase(clientId); }

  const std::shared_ptr<kj::TaskSet> mTaskSet;
  const std::string mLogName;
  kj::MutexGuarded<kj::Maybe<const kj::Executor&>> mExecutor;
  std::jthread mWorkerThread;
  std::unordered_map<std::size_t, std::unique_ptr<typename EsUtil::Callback<Result>::Client>> mClient;
};

}  // namespace cap
}  // namespace es_util

#endif  // CAPNPROTO_SERVER_PUBLISHER_HELPER_H_
