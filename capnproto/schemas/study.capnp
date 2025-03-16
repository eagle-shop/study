@0xd1f41a741501db7a;

using import "stream.capnp".Stream;

interface Study {
  const sock: Text = "/tmp/capnp_study.sock";

  fetch @0 () -> (result: Result(Text, ErrorMessage));
  subscribeX @1 (settings: Settings, callback: Callback(Text)) -> (result: Result(Stream, ErrorMessage));
  subscribeY @2 (callback: Callback(Result(Text, ErrorMessage))) -> (result: Result(Stream, ErrorMessage));

  struct Settings {
    dmy @0: Void;
  }

  interface Callback(Type) {
    send @0 (value: Type) -> stream;
  }

  struct ErrorMessage {
    message @0: Text;
  }

  struct Result(Value, Error) {
    union {
      value @0: Value;
      error @1: Error;
    }
  }
}
