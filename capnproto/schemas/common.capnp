@0xf835331bec0a2288;

interface Stream {}

interface Callback(Type) {
  send @0 (value: Type) -> stream;
}

struct Ok {
}

struct Ng {
  message @0: Text;
}

struct Result(ResultOk, ResultNg) {
  union {
    value @0: ResultOk;
    error @1: ResultNg;
  }
}
