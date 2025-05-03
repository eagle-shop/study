@0xd1f41a741501db7a;

using import "es_util.capnp".EsUtil.Stream;
using import "es_util.capnp".EsUtil.Callback;
using import "common.capnp".Ok;
using import "common.capnp".Ng;
using import "common.capnp".Result;
using import "common.capnp".Date;

interface Study {
  const sock: Text = "/tmp/capnp_study.sock";

  createUserId @0 () -> (result: Result(UserId, Ng));
  deleteUserId @1 (userId: UserId) -> (result: Result(Ok, Ng));
  subscribeX @2 (userId: UserId, callback: Callback(Text)) -> (result: Result(Stream, Ng));
  subscribeY @3 (userId: UserId, callback: Callback(Result(DailyNotification, Ng))) -> (result: Result(Stream, Ng));

  struct UserId {
    id @0: UInt64;
  }

  struct DailyNotification {
    date @0: Date;
    dmy @1: Text;
  }
}
