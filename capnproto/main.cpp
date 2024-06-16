#include "server.h"

void clientMain();

int main() {
  auto startServer = StudyServer::start();
  startServer.wait();

  clientMain();

  auto endServer = StudyServer::end();
  endServer.wait();

  return 0;
}
