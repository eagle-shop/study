#ifndef _STUDY_CAPNPROTO_SERVER_
#define _STUDY_CAPNPROTO_SERVER_

#include <capnp/ez-rpc.h>
#include <kj/async-io.h>
#include <kj/async.h>

#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>

#include "study.capnp.h"

class StudyServer final {
 public:
  static std::future<void> start();
  static std::future<void> end();

 private:
  class Server;
  class Interface final {
   public:
    Interface();
    ~Interface() = default;

    Interface(const Interface &)            = delete;
    Interface(Interface &&)                 = delete;
    Interface &operator=(const Interface &) = delete;
    Interface &operator=(Interface &&)      = delete;

    void setEzRpcServer(capnp::EzRpcServer *ezRpcServer);
    void setStudyServer(StudyServer::Server *studyServer);
    kj::WaitScope &getWaitScope();
    kj::AsyncIoProvider &getIoProvider();
    void clearTasks();

   private:
    capnp::EzRpcServer *mEzRpcServer;
    StudyServer::Server *mStudyServer;
  };

  class Client final : public Study::Stream::Server {
   public:
    enum class ApiId { subscribeXXX, none };

    explicit Client(ApiId apiId, std::size_t clientId, StudyServer::Server &studyServer);
    ~Client();

   private:
    ApiId mApiId;
    std::size_t mClientId;
    StudyServer::Server &mStudyServer;
  };

  class Server final : public Study::Server, kj::TaskSet::ErrorHandler {
   public:
    static std::future<void> startServer();

    explicit Server(Interface &interface);
    void disconnection(Client::ApiId apiId, std::size_t clientId);
    void clearTasks();

   private:
    kj::Promise<void> fetchXXX(FetchXXXContext context) final;
    kj::Promise<void> subscribeXXX(SubscribeXXXContext context) final;
    void taskFailed(kj::Exception &&e) final;

    kj::Promise<void> subscribeXXXFunc();

    Interface &mInterface;
    kj::TaskSet mTaskSet;
    std::unordered_map<Client::ApiId, std::unordered_map<std::size_t, std::unique_ptr<Study::Callback::Client>>>
        mClient;
  };

  static std::optional<std::thread> mThread;
  static std::optional<kj::PromiseFulfillerPair<void>> mPromiseFulfillerPair;
  static const kj::Executor *mExecutor;
};

#endif  // _STUDY_CAPNPROTO_SERVER_