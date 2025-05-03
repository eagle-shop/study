@0xf835331bec0a2288;

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

struct Date {
  iso8601 @0: Text;
}
