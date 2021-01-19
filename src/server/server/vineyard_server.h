/** Copyright 2020 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef SRC_SERVER_SERVER_VINEYARD_SERVER_H_
#define SRC_SERVER_SERVER_VINEYARD_SERVER_H_

#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "boost/asio.hpp"

#include "common/util/callback.h"
#include "common/util/json.h"
#include "common/util/status.h"

#include "server/memory/memory.h"
#include "server/memory/stream_store.h"

namespace vineyard {

namespace asio = boost::asio;

class IMetaService;

class IPCServer;
class RPCServer;

/**
 * @brief DeferredReq aims to defer a socket request such that the request
 * is executed only when the metadata satisfies some specific condition.
 *
 */
class DeferredReq {
 public:
  using alive_t = std::function<bool()>;
  using test_t = std::function<bool(const json& meta)>;
  using call_t = std::function<Status(const json& meta)>;

  DeferredReq(alive_t alive_fn, test_t test_fn, call_t call_fn)
      : alive_fn_(alive_fn), test_fn_(test_fn), call_fn_(call_fn) {}

  bool Alive() const;

  bool TestThenCall(const json& meta) const;

 private:
  alive_t alive_fn_;
  test_t test_fn_;
  call_t call_fn_;
};

/**
 * @brief VineyardServer is the main server of vineyard
 *
 */
class VineyardServer : public std::enable_shared_from_this<VineyardServer> {
 public:
  Status Serve();
  Status Finalize();
  inline const json& GetSpec() { return spec_; }
  inline const std::string GetDeployment() {
    return spec_["deployment"].get_ref<std::string const&>();
  }
#if BOOST_VERSION >= 106600
  inline asio::io_context& GetIOContext() { return context_; }
#else
  inline asio::io_service& GetIOContext() { return context_; }
#endif
  inline std::shared_ptr<BulkStore> GetBulkStore() { return bulk_store_; }
  inline std::shared_ptr<StreamStore> GetStreamStore() { return stream_store_; }
  static std::shared_ptr<VineyardServer> Get(const json& spec);

  void MetaReady();
  void BulkReady();
  void IPCReady();
  void RPCReady();
  void BackendReady();
  void Ready();

  Status GetData(const std::vector<ObjectID>& ids, const bool sync_remote,
                 const bool wait,
                 DeferredReq::alive_t alive,  // if connection is still alive
                 callback_t<const json&> callback);

  Status ListData(std::string const& pattern, bool const regex,
                  size_t const limit, callback_t<const json&> callback);

  Status CreateData(
      const json& tree,
      callback_t<const ObjectID, const Signature, const InstanceID> callback);

  Status Persist(const ObjectID id, callback_t<> callback);

  Status IfPersist(const ObjectID id, callback_t<const bool> callback);

  Status Exists(const ObjectID id, callback_t<const bool> callback);

  Status ShallowCopy(const ObjectID id, callback_t<const ObjectID> callback);

  Status DelData(const std::vector<ObjectID>& id, const bool force,
                 const bool deep, callback_t<> callback);

  Status DeleteBlobBatch(const std::set<ObjectID>& blobs);

  Status DeleteAllAt(const json& meta, InstanceID const instance_id);

  Status PutName(const ObjectID object_id, const std::string& name,
                 callback_t<> callback);

  Status GetName(const std::string& name, const bool wait,
                 DeferredReq::alive_t alive,  // if connection is still alive
                 callback_t<const ObjectID&> callback);

  Status DropName(const std::string& name, callback_t<> callback);

  Status MigrateObject(const ObjectID object_id,
                       callback_t<const ObjectID&> callback);

  Status ClusterInfo(callback_t<const json&> callback);

  Status InstanceStatus(callback_t<const json&> callback);

  Status ProcessDeferred(const json& meta);

  inline InstanceID instance_id() { return instance_id_; }
  inline void set_instance_id(InstanceID id) { instance_id_ = id; }

  const std::string IPCSocket();

  const std::string RPCEndpoint();

  void Stop();

  ~VineyardServer();

 private:
  explicit VineyardServer(const json& spec);

#if BOOST_VERSION >= 106600
  asio::io_context context_;
#else
  asio::io_service context_;
#endif
  json spec_;
  std::shared_ptr<IMetaService> meta_service_ptr_;
  std::unique_ptr<IPCServer> ipc_server_ptr_;
  std::unique_ptr<RPCServer> rpc_server_ptr_;

  std::list<DeferredReq> deferred_;

  std::shared_ptr<BulkStore> bulk_store_;
  std::shared_ptr<StreamStore> stream_store_;

  Status serve_status_;
  using ctx_guard = asio::executor_work_guard<asio::io_context::executor_type>;
  ctx_guard guard_;

  enum ready_t {
    kMeta = 0b1,
    kBulk = 0b10,
    kIPC = 0b100,
    kRPC = 0b1000,
    kBackendReady = 0b11,  // then we can serve ipc/rpc.
    kReady = 0b1111,
  };
  unsigned char ready_;
  bool stopped_;  // avoid invoke Stop() twice.

  InstanceID instance_id_;
};

using vs_ptr_t = std::shared_ptr<VineyardServer>;

}  // namespace vineyard

#endif  // SRC_SERVER_SERVER_VINEYARD_SERVER_H_
