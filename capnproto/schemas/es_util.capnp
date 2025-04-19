@0xc53f887fe48529b8;

interface EsUtil {
  interface Stream {}

  interface Callback(Type) {
    send @0 (value: Type) -> stream;
  }
}
