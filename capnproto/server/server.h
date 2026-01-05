// Copyright (c) 2024 eagle-shop

#ifndef CAPNPROTO_SERVER_SERVER_H_
#define CAPNPROTO_SERVER_SERVER_H_

#include <capnp/ez-rpc.h>
#include <kj/async-io.h>
#include <kj/async.h>

#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "publisher_helper.h"
#include "study.capnp.h"

class StudyServer final {
 public:
  StudyServer();
  ~StudyServer();

  StudyServer(const StudyServer&)            = delete;
  StudyServer(StudyServer&&)                 = delete;
  StudyServer& operator=(const StudyServer&) = delete;
  StudyServer& operator=(StudyServer&&)      = delete;

 private:
  class Server;
  class EzRpcServerInterface final {
   public:
    EzRpcServerInterface();
    ~EzRpcServerInterface() = default;

    EzRpcServerInterface(const EzRpcServerInterface&)            = delete;
    EzRpcServerInterface(EzRpcServerInterface&&)                 = delete;
    EzRpcServerInterface& operator=(const EzRpcServerInterface&) = delete;
    EzRpcServerInterface& operator=(EzRpcServerInterface&&)      = delete;

    void initialize(const std::shared_ptr<capnp::EzRpcServer>& ezRpcServer);

    void setStudyServer(StudyServer::Server* studyServer);
    kj::WaitScope& getWaitScope();
    kj::AsyncIoProvider& getIoProvider();
    void clearTasks();

   private:
    std::shared_ptr<capnp::EzRpcServer> mEzRpcServer;
    StudyServer::Server* mStudyServer;
  };

  class Server final : public Study::Server, public kj::TaskSet::ErrorHandler {
   public:
    explicit Server(const std::shared_ptr<EzRpcServerInterface>& ezRpcServerInterface);
    ~Server();

    void clearTasks();

   private:
    kj::Promise<void> createUserId(CreateUserIdContext context) final;
    kj::Promise<void> deleteUserId(DeleteUserIdContext context) final;
    kj::Promise<void> subscribeX(SubscribeXContext context) final;
    kj::Promise<void> subscribeY(SubscribeYContext context) final;
    void taskFailed(kj::Exception&& e) final;
    std::function<kj::WaitScope&()> getWaitScopeFunc();

    using UserId = uint64_t;

    struct UserData {};

    const std::shared_ptr<EzRpcServerInterface> mInterface;
    const std::shared_ptr<kj::TaskSet> mTaskSet;
    std::unordered_map<UserId, UserData> mUserDataList;
    std::unordered_map<std::size_t, std::unique_ptr<EsUtil::Callback<capnp::Text>::Client>> mClientX;
    std::mutex mMutex;
    const std::shared_ptr<es_util::cap::PublisherHelper<capnp::Text>> mPublisherX;
    const std::shared_ptr<es_util::cap::PublisherHelper<Result<Study::DailyNotification, Ng>>> mPublisherY;
  };

  std::thread mMainThread;
  std::shared_ptr<kj::PromiseFulfillerPair<void>> mPromiseFulfillerPair;
  kj::Own<const kj::Executor> mExecutor;
};

#endif  // CAPNPROTO_SERVER_SERVER_H_
