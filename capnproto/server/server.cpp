// Copyright (c) 2024 eagle-shop

#include "server.h"

#include <kj/async.h>
#include <kj/time.h>

#include <filesystem>
#include <future>
#include <string>
#include <utility>

#include "async_helper.h"
#include "log.h"

using namespace es;
using namespace es_util;

StudyServer::StudyServer() {
  Log::print("[server]StudyServer::StudyServer start", LOG_FILE);
  std::promise<void> setUpPromise;
  const auto setUpFuture = setUpPromise.get_future();

  mMainThread = std::thread(
      [this](std::promise<void> setUpPromise) {
        const std::string sock(Study::SOCK.get().cStr());
        const std::string unixSock = "unix:" + sock;

        std::filesystem::remove(sock);

        const auto ezRpcServerInterface = std::make_shared<EzRpcServerInterface>();
        if (!ezRpcServerInterface) {
          Log::printAndThrow("[server error]could not create EzRpcServerInterface object", LOG_FILE);
        }

        auto server = kj::heap<Server>(ezRpcServerInterface);
        if (!server) {
          Log::printAndThrow("[server error]could not create Server object", LOG_FILE);
        }

        ezRpcServerInterface->setStudyServer(server.get());
        const auto ezRpcServer = std::make_shared<capnp::EzRpcServer>(kj::mv(server), unixSock.c_str());
        if (!ezRpcServer) {
          Log::printAndThrow("[server error]could not create EzRpcServer object", LOG_FILE);
        }

        ezRpcServerInterface->initialize(ezRpcServer);
        ezRpcServer->getPort().wait(ezRpcServer->getWaitScope());
        auto promiseAndFulfiller = kj::newPromiseAndFulfiller<void>();
        mPromiseFulfiller        = kj::mv(promiseAndFulfiller.fulfiller);
        if (!mPromiseFulfiller) {
          Log::printAndThrow("[server error]could not create PromiseFulfiller object", LOG_FILE);
        }

        mExecutor = kj::getCurrentThreadExecutor().addRef();
        Log::print("[server]main loop start", LOG_FILE);
        setUpPromise.set_value();

        try {
          promiseAndFulfiller.promise.wait(ezRpcServer->getWaitScope());

          Log::print("[server]main fulfill", LOG_FILE);
          ezRpcServerInterface->cleanup();
          Log::print("[server]main loop end", LOG_FILE);
        } catch (const kj::Exception& e) {
          Log::print(std::string("[server error]main loop kj::Exception: ") + e.getDescription().cStr(), LOG_FILE);
        } catch (const std::exception& e) {
          Log::print(std::string("[server error]main loop std::exception: ") + e.what(), LOG_FILE);
        } catch (...) {
          Log::print("[server error]main loop unknouwn exception", LOG_FILE);
        }
      },
      std::move(setUpPromise));

  setUpFuture.wait();
}

StudyServer::~StudyServer() {
  if ((mExecutor) && mExecutor->isLive()) {
    Log::print("[server]StudyServer::~StudyServer try to executeSync", LOG_FILE);
    mExecutor->executeSync([this]() {
      if (mPromiseFulfiller) {
        mPromiseFulfiller->fulfill();
      }
    });
    Log::print("[server]StudyServer::~StudyServer executeSync end", LOG_FILE);
  } else {
    Log::print("[server]StudyServer::~StudyServer executor is null", LOG_FILE);
  }

  if (mMainThread.joinable()) {
    Log::print("[server]StudyServer::~StudyServer try to join", LOG_FILE);
    mMainThread.join();
  }
  Log::print("[server]StudyServer::~StudyServer end", LOG_FILE);
}

StudyServer::EzRpcServerInterface::EzRpcServerInterface() : mStudyServer(nullptr) {}

StudyServer::EzRpcServerInterface::~EzRpcServerInterface() {
  Log::print("[server]EzRpcServerInterface::~EzRpcServerInterface", LOG_FILE);
}

void StudyServer::EzRpcServerInterface::initialize(const std::weak_ptr<capnp::EzRpcServer>& ezRpcServer) {
  mEzRpcServer = ezRpcServer;
}

void StudyServer::EzRpcServerInterface::setStudyServer(StudyServer::Server* studyServer) { mStudyServer = studyServer; }

kj::WaitScope& StudyServer::EzRpcServerInterface::getWaitScope() {
  const auto ins = mEzRpcServer.lock();
  if (!ins) {
    Log::printAndThrow("[server error]getWaitScope EzRpcServer is null", LOG_FILE);
  }

  return ins->getWaitScope();
}

kj::AsyncIoProvider& StudyServer::EzRpcServerInterface::getIoProvider() {
  const auto ins = mEzRpcServer.lock();
  if (!ins) {
    Log::printAndThrow("[server error]getIoProvider EzRpcServer is null", LOG_FILE);
  }

  return ins->getIoProvider();
}

void StudyServer::EzRpcServerInterface::cleanup() {
  if (mStudyServer == nullptr) {
    return;
  }

  mStudyServer->cleanup();
  mStudyServer = nullptr;
}

StudyServer::Server::Server(const std::shared_ptr<EzRpcServerInterface>& ezRpcServerInterface)
    : mInterface(ezRpcServerInterface),
      mTaskSet(std::make_shared<kj::TaskSet>(*this)),
      mPublisherX(cap::PublisherHelper<capnp::Text>::create(mTaskSet, getWaitScopeFunc(), "subscribeX")),
      mPublisherY(cap::PublisherHelper<Result<Study::DailyNotification, Ng>>::create(mTaskSet, getWaitScopeFunc(),
                                                                                     "subscribeY")) {
  Log::print("[server]Server::Server", LOG_FILE);
}

StudyServer::Server::~Server() { Log::print("[server]Server::~Server", LOG_FILE); }

void StudyServer::Server::cleanup() {
  Log::print("[server]StudyServer::Server::cleanup start", LOG_FILE);
  mPublisherX->stopWorker();
  Log::print("[server]StudyServer::Server::cleanup mPublisherX stopWorker end", LOG_FILE);
  mPublisherX.reset();
  Log::print("[server]StudyServer::Server::cleanup mPublisherX reset end", LOG_FILE);
  mPublisherY->stopWorker();
  Log::print("[server]StudyServer::Server::cleanup mPublisherY stopWorker end", LOG_FILE);
  mPublisherY.reset();
  Log::print("[server]StudyServer::Server::cleanup mPublisherY reset end", LOG_FILE);
  if (mTaskSet && mInterface) {
    Log::print("[server]StudyServer::Server::cleanup wait", LOG_FILE);
    mTaskSet->onEmpty().wait(mInterface->getWaitScope());
    Log::print("[server]StudyServer::Server::cleanup OK", LOG_FILE);
  }
}

kj::Promise<void> StudyServer::Server::createUserId(CreateUserIdContext context) {
  Log::print("[server]createUserId start", LOG_FILE);

  return cap::AsyncHelper::executeAsync(
      [this]() {
        const std::lock_guard<std::mutex> lock(mMutex);
        auto ret = mUserDataList.emplace(mUserDataList.size(), UserData{});
        return ret.second ? std::optional<UserId>(mUserDataList.size() - 1) : std::nullopt;
      },
      [context = kj::mv(context)](std::optional<std::optional<UserId>>&& result) mutable {
        if (result && result.value()) {
          context.getResults().initResult().initValue().setId(result.value().value());
          Log::print("[server]createUserId end. id: " + std::to_string(result.value().value()), LOG_FILE);
        } else {
          context.getResults().initResult().initError().setMessage("createUserId failed");
        }
      });
}

kj::Promise<void> StudyServer::Server::deleteUserId(DeleteUserIdContext context) {
  Log::print("[server]deleteUserId start", LOG_FILE);

  if (!context.getParams().hasUserId()) {
    context.getResults().initResult().initError().setMessage("deleteUserId id is null");
    return kj::READY_NOW;
  }

  Log::print("[server]deleteUserId start. id: " + std::to_string(context.getParams().getUserId().getId()), LOG_FILE);

  return cap::AsyncHelper::executeAsync(
      [this, id = context.getParams().getUserId().getId()]() {
        const std::lock_guard<std::mutex> lock(mMutex);
        if (mUserDataList.count(id) > 0) {
          mUserDataList.erase(id);
          return true;
        } else {
          return false;
        }
      },
      [context = kj::mv(context)](std::optional<bool>&& result) mutable {
        if (result && result.value()) {
          context.getResults().initResult().initValue();
        } else {
          context.getResults().initResult().initError().setMessage("deleteUserId invalid id");
        }
        Log::print("[server]deleteUserId end", LOG_FILE);
      });
}

kj::Promise<void> StudyServer::Server::subscribeX(SubscribeXContext context) {
  Log::print("[server]subscribeX start", LOG_FILE);

  if (!context.getParams().hasCallback()) {
    context.getResults().initResult().initError().setMessage("hasCallback is false");
    return kj::READY_NOW;
  }

  auto callback = std::make_unique<EsUtil::Callback<capnp::Text>::Client>(context.getParams().getCallback());
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

    if (!mPublisherX->isWorkerRunning()) {
      auto ret = mPublisherX->setWorker([this](const std::atomic<bool>& stopFlag) {
        while (!stopFlag.load()) {
          Log::print("[server]subscribeX worker try publish", LOG_FILE);
          mPublisherX->publish("send X");
          Log::print("[server]subscribeX worker end publish", LOG_FILE);
        }
        Log::print("[server]subscribeX worker thread end", LOG_FILE);
      });

      if (!ret) {
        context.getResults().initResult().initError().setMessage("could not create worker object");
        return kj::READY_NOW;
      }
    }
    context.getResults().initResult().setValue(kj::mv(client));
  }
  Log::print("[server]subscribeX registered.", LOG_FILE);
  return kj::READY_NOW;
}

kj::Promise<void> StudyServer::Server::subscribeY(SubscribeYContext context) {
  Log::print("[server]subscribeY start", LOG_FILE);

  if (!context.getParams().hasCallback()) {
    context.getResults().initResult().initError().setMessage("hasCallback is false");
    return kj::READY_NOW;
  }

  auto callback = std::make_unique<EsUtil::Callback<Result<Study::DailyNotification, Ng>>::Client>(
      context.getParams().getCallback());
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

    if (!mPublisherY->isWorkerRunning()) {
      auto ret = mPublisherY->setWorker([this](const std::atomic<bool>& stopFlag) {
        while (!stopFlag.load()) {
          capnp::MallocMessageBuilder resultMessageBuilder;
          auto result = resultMessageBuilder.initRoot<Result<Study::DailyNotification, Ng>>();
          result.initValue().initDate().setIso8601("2000-01-01T00:00:00Z");
          result.getValue().setDmy("send Y");
          Log::print("[server]subscribeY worker try publish", LOG_FILE);
          mPublisherY->publish(kj::mv(result));
          Log::print("[server]subscribeY worker end publish", LOG_FILE);
        }
        Log::print("[server]subscribeY worker thread end", LOG_FILE);
      });

      if (!ret) {
        context.getResults().initResult().initError().setMessage("could not create worker object");
        return kj::READY_NOW;
      }
    }
    context.getResults().initResult().setValue(kj::mv(client));
  }
  Log::print("[server]subscribeY registered.", LOG_FILE);
  return kj::READY_NOW;
}

void StudyServer::Server::taskFailed(kj::Exception&& e) {
  Log::print(std::string("[server]taskFailed: ") + e.getDescription().cStr(), LOG_FILE);
}

std::function<kj::WaitScope&()> StudyServer::Server::getWaitScopeFunc() {
  const std::weak_ptr<EzRpcServerInterface> interface = mInterface;
  return [interface]() -> kj::WaitScope& {
    const auto ins = interface.lock();
    if (!ins) {
      Log::printAndThrow("[server error]EzRpcServerInterface is null", LOG_FILE);
    }
    return ins->getWaitScope();
  };
}
