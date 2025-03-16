// Copyright (c) 2024 eagle-shop

#include "server.h"

#include <kj/async.h>
#include <kj/time.h>

#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <utility>

#include "log.h"

StudyServer::StudyServer() {
  std::promise<void> setUpPromise;
  const auto setUpFuture = setUpPromise.get_future();

  mMainThread = std::thread(
      [this](std::promise<void> setUpPromise) {
        const std::string sock(Study::SOCK.get().cStr());
        const std::string unixSock = "unix:" + sock;

        std::filesystem::remove(sock);

        const auto ezRpcServerInterface = std::make_shared<EzRpcServerInterface>();
        if (!ezRpcServerInterface) {
          Log::printAndThrow("[server error]could not create EzRpcServerInterface object");
        }

        auto server = kj::heap<Server>(ezRpcServerInterface);
        if (!server) {
          Log::printAndThrow("[server error]could not create Server object");
        }

        ezRpcServerInterface->setStudyServer(server.get());
        const auto ezRpcServer = std::make_shared<capnp::EzRpcServer>(kj::mv(server), unixSock.c_str());
        if (!ezRpcServer) {
          Log::printAndThrow("[server error]could not create EzRpcServer object");
        }

        ezRpcServerInterface->initialize(ezRpcServer);
        ezRpcServer->getPort().wait(ezRpcServer->getWaitScope());
        mPromiseFulfillerPair = std::make_shared<kj::PromiseFulfillerPair<void>>(kj::newPromiseAndFulfiller<void>());
        if (!mPromiseFulfillerPair) {
          Log::printAndThrow("[server error]could not create PromiseFulfillerPair object");
        }

        mExecutor = kj::getCurrentThreadExecutor().addRef();
        Log::print("[server]main loop start");
        setUpPromise.set_value();

        try {
          mPromiseFulfillerPair->promise.wait(ezRpcServer->getWaitScope());

          Log::print("[server]main fulfill");
          ezRpcServerInterface->clearTasks();
          Log::print("[server]main loop end");
        } catch (kj::Exception &e) {
          Log::print(std::string("[server error]main loop kj::Exception: ") + e.getDescription().cStr());
        } catch (std::exception &e) {
          Log::print(std::string("[server error]main loop std::exception: ") + e.what());
        } catch (...) {
          Log::print("[server error]main loop unknouwn exception");
        }
      },
      std::move(setUpPromise));

  setUpFuture.wait();
}

StudyServer::~StudyServer() {
  if ((mExecutor) && mExecutor->isLive()) {
    Log::print("[server]StudyServer::~StudyServer try to executeSync");
    mExecutor->executeSync([this]() {
      if (mPromiseFulfillerPair && mPromiseFulfillerPair->fulfiller) {
        mPromiseFulfillerPair->fulfiller->fulfill();
      }
    });
    Log::print("[server]StudyServer::~StudyServer executeSync end");
  } else {
    Log::print("[server]StudyServer::~StudyServer executor is null");
  }

  if (mMainThread.joinable()) {
    Log::print("[server]StudyServer::~StudyServer try to join");
    mMainThread.join();
  }
  Log::print("[server]StudyServer::~StudyServer end");
}

StudyServer::EzRpcServerInterface::EzRpcServerInterface() : mStudyServer(nullptr) {}

void StudyServer::EzRpcServerInterface::initialize(const std::weak_ptr<capnp::EzRpcServer> &ezRpcServer) {
  mEzRpcServer = ezRpcServer;
}

void StudyServer::EzRpcServerInterface::setStudyServer(StudyServer::Server *studyServer) { mStudyServer = studyServer; }

kj::WaitScope &StudyServer::EzRpcServerInterface::getWaitScope() {
  auto ins = mEzRpcServer.lock();
  if (!ins) {
    Log::printAndThrow("[server]EzRpcServer is null");
  }

  return ins->getWaitScope();
}

kj::AsyncIoProvider &StudyServer::EzRpcServerInterface::getIoProvider() {
  auto ins = mEzRpcServer.lock();
  if (!ins) {
    Log::printAndThrow("[server]EzRpcServer is null");
  }

  return ins->getIoProvider();
}

void StudyServer::EzRpcServerInterface::clearTasks() {
  if (mStudyServer == nullptr) {
    return;
  }

  mStudyServer->clearTasks();
}

StudyServer::Client::Client(std::size_t clientId, StudyServer::Server &studyServer)
    : mClientId(clientId), mStudyServer(studyServer) {}

StudyServer::Client::~Client() { mStudyServer.disconnection(mClientId); }

StudyServer::Server::Server(const std::weak_ptr<EzRpcServerInterface> &ezRpcServerInterface)
    : mInterface(ezRpcServerInterface), mTaskSet(*this), mClientCounter(0) {
  auto ins = mInterface.lock();
  if (!ins) {
    Log::printAndThrow("[server error]EzRpcServerInterface is null");
  }
}

StudyServer::Server::~Server() {
  for (auto &e : mThreads) {
    Log::print("[server]Server::~Server try to join");
    e.join();
  }
  Log::print("[server]Server::~Server end");
}

void StudyServer::Server::disconnection(std::size_t clientId) {
  Log::print("[server]disconnection(" + std::to_string(clientId) + ")");
  if (mClientX.contains(clientId)) {
    mClientX.erase(clientId);
  } else if (mClientY.contains(clientId)) {
    mClientY.erase(clientId);
  }
}

void StudyServer::Server::clearTasks() {
  mTaskSet.clear();
  auto ins = mInterface.lock();
  if (ins) {
    Log::print("[server]StudyServer::Server::clearTasks wait");
    mTaskSet.onEmpty().wait(ins->getWaitScope());
    Log::print("[server]StudyServer::Server::clearTasks ok");
  }
}

kj::Promise<void> StudyServer::Server::fetch(FetchContext context) {
  context.getResults().initResult().setValue("fetch OK");
  return kj::READY_NOW;
}

kj::Promise<void> StudyServer::Server::subscribeX(SubscribeXContext context) {
  if (!context.getParams().hasCallback()) {
    context.getResults().initResult().initError().setMessage("hasCallback is false");
    return kj::READY_NOW;
  }

  auto callback = std::make_unique<Study::Callback<capnp::Text>::Client>(context.getParams().getCallback());
  if (!callback) {
    context.getResults().initResult().initError().setMessage("could not hold callback object");
    return kj::READY_NOW;
  }

  auto client = kj::heap<Client>(mClientCounter, *this);
  if (!client) {
    context.getResults().initResult().initError().setMessage("could not create client object");
    return kj::READY_NOW;
  }

  context.getResults().initResult().setValue(kj::mv(client));
  if (mClientX.empty()) {
    mTaskSet.add(subscribeXFunc());
  }
  mClientX.emplace(mClientCounter, std::move(callback));

  Log::print("[server]subscribeX registered. id: " + std::to_string(mClientCounter));
  mClientCounter++;

  return kj::READY_NOW;
}

kj::Promise<void> StudyServer::Server::subscribeY(SubscribeYContext context) {
  if (!context.getParams().hasCallback()) {
    context.getResults().initResult().initError().setMessage("hasCallback is false");
    return kj::READY_NOW;
  }

  auto callback = std::make_unique<Study::Callback<::Study::Result<::capnp::Text, ::Study::ErrorMessage>>::Client>(
      context.getParams().getCallback());
  if (!callback) {
    context.getResults().initResult().initError().setMessage("could not hold callback object");
    return kj::READY_NOW;
  }

  auto client = kj::heap<Client>(mClientCounter, *this);
  if (!client) {
    context.getResults().initResult().initError().setMessage("could not create client object");
    return kj::READY_NOW;
  }

  context.getResults().initResult().setValue(kj::mv(client));
  if (mClientY.empty()) {
    if (!mExecutor) {
      mExecutor = kj::getCurrentThreadExecutor().addRef();
    }

    mThreads.push_back(std::thread([this]() {
      while (true) {
        if ((mExecutor) && mExecutor->isLive()) {
          try {
            Log::print("[server]subscribeY try to executeSync");
            mExecutor->executeSync([this]() {
              for (const auto &e : mClientY) {
                if (e.second) {
                  auto callback = e.second->sendRequest();
                  callback.getValue().setValue("send Y");
                  mTaskSet.add(callback.send()
                                   .then([]() { Log::print("[server]subscribeY callback.send() OK"); },
                                         [](kj::Exception &&e) {
                                           Log::print(std::string("[server]subscribeY callback.send() Exception: ") +
                                                      e.getDescription().cStr());
                                         })
                                   .attach(kj::mv(callback)));
                }
              }
            });
            Log::print("[server]subscribeY executeSync end");
          } catch (kj::Exception &e) {
            Log::print(std::string("[server]subscribeY kj::Exception: ") + e.getDescription().cStr());
            return;
          } catch (std::exception &e) {
            Log::print(std::string("[server]subscribeY std::exception: ") + e.what());
            return;
          } catch (...) {
            Log::print("[server]subscribeY unknown exception");
            return;
          }
        } else {
          Log::print("[server]subscribeY executor is null");
          return;
        }
        Log::print("[server]subscribeY free lock");
      }
    }));
  }
  mClientY.emplace(mClientCounter, std::move(callback));

  Log::print("[server]subscribeY registered. id: " + std::to_string(mClientCounter));
  mClientCounter++;

  return kj::READY_NOW;
}

void StudyServer::Server::taskFailed(kj::Exception &&e) {
  Log::print(std::string("[server]taskFailed: ") + e.getDescription().cStr());
}

kj::Promise<void> StudyServer::Server::subscribeXFunc() {
  auto ins = mInterface.lock();
  return ins ? (ins->getIoProvider().getTimer().afterDelay(1 * kj::SECONDS).then([this]() {
    if (mClientX.empty()) {
      return;
    }

    for (const auto &e : mClientX) {
      if (e.second) {
        Log::print("[server]subscribeXFunc send");
        auto callback = e.second->sendRequest();
        callback.setValue("send X");
        mTaskSet.add(callback.send()
                         .then([]() { Log::print("[server]subscribeX callback.send() OK"); },
                               [](kj::Exception &&e) {
                                 Log::print(std::string("[server]subscribeX callback.send() Exception: ") +
                                            e.getDescription().cStr());
                               })
                         .attach(kj::mv(callback)));
      }
    }
    mTaskSet.add(subscribeXFunc());
  }))
             : kj::READY_NOW;
}
