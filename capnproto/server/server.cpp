#include "server.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

std::optional<std::thread> StudyServer::mThread                                  = std::nullopt;
std::optional<kj::PromiseFulfillerPair<void>> StudyServer::mPromiseFulfillerPair = std::nullopt;
const kj::Executor *StudyServer::mExecutor                                       = nullptr;

std::future<void> StudyServer::start() {
  if (mThread) {
    return std::future<void>();
  }

  std::promise<void> promise;
  auto future = promise.get_future();
  mThread     = std::thread(
      [](std::promise<void> promise) {
        try {
          const std::string sock(Study::SOCK.get().cStr());
          const std::string unixSock = "unix:" + sock;

          std::filesystem::remove(sock);

          Interface interface;
          capnp::EzRpcServer server(kj::heap<Server>(interface), unixSock.c_str());

          interface.setEzRpcServer(&server);
          server.getPort().wait(server.getWaitScope());
          mPromiseFulfillerPair = kj::newPromiseAndFulfiller<void>();
          mExecutor             = &kj::getCurrentThreadExecutor();

          std::cout << "[server]main loop start" << std::endl;
          promise.set_value();
          mPromiseFulfillerPair.value().promise.wait(server.getWaitScope());

          interface.clearTasks();
          mPromiseFulfillerPair.reset();
          mExecutor = nullptr;
          std::cout << "[server]main loop end" << std::endl;
        } catch (...) {
          std::cout << "[server]catch exception" << std::endl;
          promise.set_exception(std::current_exception());
        }
      },
      std::move(promise));

  return future;
}

std::future<void> StudyServer::end() {
  if (!mThread || !mPromiseFulfillerPair || (mExecutor == nullptr) || !mExecutor->isLive()) {
    return std::future<void>();
  }

  std::promise<void> promise;
  auto future = promise.get_future();
  std::thread(
      [&](std::promise<void> promise) {
        mExecutor->executeSync([&]() {
          if (mPromiseFulfillerPair.value().fulfiller) {
            mPromiseFulfillerPair.value().fulfiller->fulfill();
          }
        });
        mThread.value().join();
        mThread.reset();
        promise.set_value();
      },
      std::move(promise))
      .detach();

  return future;
}

StudyServer::Interface::Interface() : mEzRpcServer(nullptr), mStudyServer(nullptr) {}

void StudyServer::Interface::setEzRpcServer(capnp::EzRpcServer *ezRpcServer) { mEzRpcServer = ezRpcServer; }

void StudyServer::Interface::setStudyServer(StudyServer::Server *studyServer) { mStudyServer = studyServer; }

kj::WaitScope &StudyServer::Interface::getWaitScope() {
  if (mEzRpcServer == nullptr) {
    throw(std::exception_ptr());
  }

  return mEzRpcServer->getWaitScope();
}

kj::AsyncIoProvider &StudyServer::Interface::getIoProvider() {
  if (mEzRpcServer == nullptr) {
    throw(std::exception_ptr());
  }

  return mEzRpcServer->getIoProvider();
}

void StudyServer::Interface::clearTasks() {
  if (mStudyServer == nullptr) {
    return;
  }

  mStudyServer->clearTasks();
}

StudyServer::Client::Client(ApiId apiId, std::size_t clientId, StudyServer::Server &studyServer)
    : mApiId(apiId), mClientId(clientId), mStudyServer(studyServer) {}

StudyServer::Client::~Client() { mStudyServer.disconnection(mApiId, mClientId); }

StudyServer::Server::Server(Interface &interface) : mInterface(interface), mTaskSet(*this) {
  interface.setStudyServer(this);
}

void StudyServer::Server::disconnection(Client::ApiId apiId, std::size_t clientId) {
  std::cout << "[server]disconnection(" << static_cast<uint64_t>(apiId) << ", " << clientId << ")" << std::endl;
  if (mClient.contains(apiId)) {
    if (mClient[apiId].contains(clientId)) {
      mClient[apiId].erase(clientId);
      if (mClient[apiId].empty()) {
        mClient.erase(apiId);
      }
    }
  }
}

void StudyServer::Server::clearTasks() { mTaskSet.clear(); }

kj::Promise<void> StudyServer::Server::fetchXXX(FetchXXXContext context) {
  std::string str("fetchXXX OK");
  context.getResults().getResult().setValue(str.c_str());

  return kj::READY_NOW;
}

kj::Promise<void> StudyServer::Server::subscribeXXX(SubscribeXXXContext context) {
  if (!context.getParams().hasCallback()) {
    return kj::READY_NOW;
  }

  auto callback = std::make_unique<Study::Callback::Client>(context.getParams().getCallback());
  if (!callback) {
    return kj::READY_NOW;
  }

  const auto id = mClient.contains(Client::ApiId::subscribeXXX) ? mClient[Client::ApiId::subscribeXXX].size() : 0;
  auto client   = kj::heap<Client>(Client::ApiId::subscribeXXX, id, *this);
  if (!client) {
    return kj::READY_NOW;
  }

  context.getResults().getResult().setValue(kj::mv(client));
  if (!mClient.contains(Client::ApiId::subscribeXXX)) {
    std::unordered_map<std::size_t, std::unique_ptr<Study::Callback::Client>> callbacks;
    callbacks.emplace(id, std::move(callback));
    mClient.emplace(Client::ApiId::subscribeXXX, std::move(callbacks));
    mTaskSet.add(subscribeXXXFunc());
  } else {
    mClient[Client::ApiId::subscribeXXX].emplace(id, std::move(callback));
  }

  std::cout << "[server]subscribeXXX registered. id: " << id << std::endl;

  return kj::READY_NOW;
}

void StudyServer::Server::taskFailed(kj::Exception &&e) {
  std::cout << "[server]taskFailed: " << e.getDescription().cStr() << std::endl;
}

kj::Promise<void> StudyServer::Server::subscribeXXXFunc() {
  auto promise = mInterface.getIoProvider().getTimer().afterDelay(1 * kj::SECONDS);
  return promise.then([this]() {
    if (!mClient.contains(Client::ApiId::subscribeXXX)) {
      return;
    }

    for (const auto &e : mClient[Client::ApiId::subscribeXXX]) {
      if (e.second) {
        std::cout << "[server]subscribeXXXFunc send" << std::endl;
        auto callback = e.second->sendTextRequest();
        callback.setText("test");
        mTaskSet.add(callback.send().then([]() { std::cout << "[server]callback.send() OK" << std::endl; },
                                          [](kj::Exception &&e) {
                                            std::cout
                                                << "[server]callback.send() Exception:" << e.getDescription().cStr()
                                                << std::endl;
                                          }));
      }
    }
    mTaskSet.add(subscribeXXXFunc());
  });
}
