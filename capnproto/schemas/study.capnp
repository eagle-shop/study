@0xd1f41a741501db7a;

interface Study {
  const sock: Text = "/tmp/capnp_study.sock";

  fetchXXX @0 () -> (result: Result(Text, ErrorMessage));
  subscribeXXX @1 (settings: Settings, callback: Callback) -> (result: Result(Stream, ErrorMessage));

  struct Settings {
    dmy @0: Void;
  }

  interface Stream {}

  interface Callback {
    sendText @0 (text: Text) -> stream;
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
