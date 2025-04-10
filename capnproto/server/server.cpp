// Copyright (c) 2024 eagle-shop

#include "server.h"

#include <kj/async.h>
#include <kj/time.h>

#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "async_helper.h"
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

StudyServer::Server::Server(const std::weak_ptr<EzRpcServerInterface> &ezRpcServerInterface)
    : mInterface(ezRpcServerInterface),
      mTaskSet(std::make_shared<kj::TaskSet>(*this)),
      mPublisherX(es_util::cap::PublisherHelper<capnp::Text>::create(mTaskSet, "subscribeX")),
      mPublisherY(es_util::cap::PublisherHelper<Result<capnp::Text, Ng>>::create(mTaskSet, "subscribeY")) {
  auto ins = mInterface.lock();
  if (!ins) {
    Log::printAndThrow("[server error]EzRpcServerInterface is null");
  }
}

StudyServer::Server::~Server() { Log::print("[server]Server::~Server end"); }

void StudyServer::Server::clearTasks() {
  if (mTaskSet) {
    mTaskSet->clear();

    auto ins = mInterface.lock();
    if (ins) {
      Log::print("[server]StudyServer::Server::clearTasks wait");
      mTaskSet->onEmpty().wait(ins->getWaitScope());
      Log::print("[server]StudyServer::Server::clearTasks OK");
    }
  }
}

kj::Promise<void> StudyServer::Server::createUserId(CreateUserIdContext context) {
  Log::print("[server]createUserId start");

  return es_util::cap::AsyncHelper::executeAsync(
      [this]() {
        std::lock_guard<std::mutex> lock(mMutex);
        auto ret = mUserDataList.emplace(mUserDataList.size(), UserData{});
        return ret.second ? std::optional<UserId>(mUserDataList.size() - 1) : std::nullopt;
      },
      [context = kj::mv(context)](std::optional<std::optional<UserId>> &&result) mutable {
        if (result && result.value()) {
          context.getResults().initResult().initValue().setId(result.value().value());
          Log::print("[server]createUserId end. id: " + std::to_string(result.value().value()));
        } else {
          context.getResults().initResult().initError().setMessage("createUserId failed");
        }
      });
}

kj::Promise<void> StudyServer::Server::deleteUserId(DeleteUserIdContext context) {
  if (!context.getParams().hasUserId()) {
    context.getResults().initResult().initError().setMessage("deleteUserId id is null");
    return kj::READY_NOW;
  }

  Log::print("[server]deleteUserId start. id: " + std::to_string(context.getParams().getUserId().getId()));

  return es_util::cap::AsyncHelper::executeAsync(
      [this, id = context.getParams().getUserId().getId()]() {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mUserDataList.contains(id)) {
          mUserDataList.erase(id);
          return true;
        } else {
          return false;
        }
      },
      [context = kj::mv(context)](std::optional<bool> &&result) mutable {
        if (result && result.value()) {
          context.getResults().initResult().initValue();
        } else {
          context.getResults().initResult().initError().setMessage("deleteUserId invalid id");
        }
        Log::print("[server]deleteUserId end");
      });
}

kj::Promise<void> StudyServer::Server::subscribeX(SubscribeXContext context) {
  if (!context.getParams().hasCallback()) {
    context.getResults().initResult().initError().setMessage("hasCallback is false");
    return kj::READY_NOW;
  }

  auto callback = std::make_unique<Callback<capnp::Text>::Client>(context.getParams().getCallback());
  if (!callback) {
    context.getResults().initResult().initError().setMessage("could not hold callback object");
    return kj::READY_NOW;
  }

  if (mPublisherX) {
    auto client = mPublisherX->addSubscriber(std::move(callback));
    if (!client) {
      context.getResults().initResult().initError().setMessage("could not create client object");
      return kj::READY_NOW;
    }

    if (!(*mPublisherX)) {
      auto ret = mPublisherX->setWorker([this](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
          mPublisherX->publish("send X");
        }
      });

      if (!ret) {
        context.getResults().initResult().initError().setMessage("could not create worker object");
        return kj::READY_NOW;
      }
    }
    context.getResults().initResult().setValue(kj::mv(client));
  }
  Log::print("[server]subscribeX registered.");
  return kj::READY_NOW;
}

kj::Promise<void> StudyServer::Server::subscribeY(SubscribeYContext context) {
  if (!context.getParams().hasCallback()) {
    context.getResults().initResult().initError().setMessage("hasCallback is false");
    return kj::READY_NOW;
  }

  auto callback = std::make_unique<Callback<Result<capnp::Text, Ng>>::Client>(context.getParams().getCallback());
  if (!callback) {
    context.getResults().initResult().initError().setMessage("could not hold callback object");
    return kj::READY_NOW;
  }

  if (mPublisherY) {
    auto client = mPublisherY->addSubscriber(std::move(callback));
    if (!client) {
      context.getResults().initResult().initError().setMessage("could not create client object");
      return kj::READY_NOW;
    }

    if (!(*mPublisherY)) {
      auto ret = mPublisherY->setWorker([this](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
          capnp::MallocMessageBuilder resultMessageBuilder;
          auto result = resultMessageBuilder.initRoot<Result<capnp::Text, Ng>>();
          result.setValue("send Y");
          mPublisherY->publish(kj::mv(result));
        }
      });

      if (!ret) {
        context.getResults().initResult().initError().setMessage("could not create worker object");
        return kj::READY_NOW;
      }
    }
    context.getResults().initResult().setValue(kj::mv(client));
  }
  Log::print("[server]subscribeY registered.");
  return kj::READY_NOW;
}

void StudyServer::Server::taskFailed(kj::Exception &&e) {
  Log::print(std::string("[server]taskFailed: ") + e.getDescription().cStr());
}
