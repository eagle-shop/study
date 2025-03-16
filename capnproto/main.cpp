// Copyright (c) 2024 eagle-shop

#include "log.h"
#include "server.h"

void clientMain();

int main() {
  {
    StudyServer studyServer;
    clientMain();
  }
  Log::print("[main]end");
  return 0;
}
