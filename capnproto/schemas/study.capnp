@0xd1f41a741501db7a;

using import "common.capnp".Stream;
using import "common.capnp".Callback;
using import "common.capnp".Ok;
using import "common.capnp".Ng;
using import "common.capnp".Result;

interface Study {
  const sock: Text = "/tmp/capnp_study.sock";

  createUserId @0 () -> (result: Result(UserId, Ng));
  deleteUserId @1 (userId: UserId) -> (result: Result(Ok, Ng));
  subscribeX @2 (userId: UserId, callback: Callback(Text)) -> (result: Result(Stream, Ng));
  subscribeY @3 (userId: UserId, callback: Callback(Result(Text, Ng))) -> (result: Result(Stream, Ng));

  struct UserId {
    id @0: UInt64;
  }
}
