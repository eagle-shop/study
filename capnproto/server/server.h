// Copyright (c) 2024 eagle-shop

#ifndef CAPNPROTO_SERVER_SERVER_H_
#define CAPNPROTO_SERVER_SERVER_H_

#include <capnp/ez-rpc.h>
#include <kj/async-io.h>
#include <kj/async.h>

#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "study.capnp.h"

class StudyServer final {
 public:
  StudyServer();
  ~StudyServer();

  StudyServer(const StudyServer &)            = delete;
  StudyServer(StudyServer &&)                 = delete;
  StudyServer &operator=(const StudyServer &) = delete;
  StudyServer &operator=(StudyServer &&)      = delete;

 private:
  class Server;
  class EzRpcServerInterface final {
   public:
    EzRpcServerInterface();
    ~EzRpcServerInterface() = default;

    EzRpcServerInterface(const EzRpcServerInterface &)            = delete;
    EzRpcServerInterface(EzRpcServerInterface &&)                 = delete;
    EzRpcServerInterface &operator=(const EzRpcServerInterface &) = delete;
    EzRpcServerInterface &operator=(EzRpcServerInterface &&)      = delete;

    void initialize(const std::weak_ptr<capnp::EzRpcServer> &ezRpcServer);

    void setStudyServer(StudyServer::Server *studyServer);
    kj::WaitScope &getWaitScope();
    kj::AsyncIoProvider &getIoProvider();
    void clearTasks();

   private:
    std::weak_ptr<capnp::EzRpcServer> mEzRpcServer;
    StudyServer::Server *mStudyServer;
  };

  class Client final : public Stream::Server {
   public:
    explicit Client(std::size_t clientId, StudyServer::Server &studyServer);
    virtual ~Client();

   private:
    const std::size_t mClientId;
    StudyServer::Server &mStudyServer;
  };

  class Server final : public Study::Server, public kj::TaskSet::ErrorHandler {
   public:
    explicit Server(const std::weak_ptr<EzRpcServerInterface> &ezRpcServerInterface);
    virtual ~Server();

    void disconnection(std::size_t clientId);
    void clearTasks();

   private:
    kj::Promise<void> createUserId(CreateUserIdContext context) final;
    kj::Promise<void> deleteUserId(DeleteUserIdContext context) final;
    kj::Promise<void> subscribeX(SubscribeXContext context) final;
    kj::Promise<void> subscribeY(SubscribeYContext context) final;
    void taskFailed(kj::Exception &&e) final;

    template <typename F1, typename F2>
    kj::Promise<void> executeAsync(F1 &&func1, F2 &&func2);
    kj::Promise<void> subscribeXFunc();

    struct UserData {};

    const std::weak_ptr<EzRpcServerInterface> mInterface;
    kj::TaskSet mTaskSet;
    kj::Own<const kj::Executor> mExecutor;
    std::list<std::thread> mThreads;
    std::unordered_map<uint64_t, UserData> mUserDataList;
    uint64_t mClientCounter;
    std::unordered_map<std::size_t, std::unique_ptr<Callback<capnp::Text>::Client>> mClientX;
    std::unordered_map<std::size_t, std::unique_ptr<Callback<Result<capnp::Text, Ng>>::Client>> mClientY;
    std::mutex mMutex;
  };

  std::thread mMainThread;
  std::shared_ptr<kj::PromiseFulfillerPair<void>> mPromiseFulfillerPair;
  kj::Own<const kj::Executor> mExecutor;
};

#endif  // CAPNPROTO_SERVER_SERVER_H_
