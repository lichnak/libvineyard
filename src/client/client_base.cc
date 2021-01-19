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

#include "client/client_base.h"

#include <utility>

#include "boost/range/combine.hpp"

#include "client/io.h"
#include "client/utils.h"
#include "common/util/protocols.h"

namespace vineyard {

ClientBase::ClientBase() : connected_(false), vineyard_conn_(0) {}

Status ClientBase::GetData(const ObjectID id, json& tree,
                           const bool sync_remote, const bool wait) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteGetDataRequest(id, sync_remote, wait, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadGetDataReply(message_in, tree));
  return Status::OK();
}

Status ClientBase::GetData(const std::vector<ObjectID>& ids,
                           std::vector<json>& trees, const bool sync_remote,
                           const bool wait) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteGetDataRequest(ids, sync_remote, wait, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  std::unordered_map<ObjectID, json> meta_trees;
  RETURN_ON_ERROR(ReadGetDataReply(message_in, meta_trees));
  trees.reserve(ids.size());
  for (auto const& id : ids) {
    trees.emplace_back(meta_trees.at(id));
  }
  return Status::OK();
}

Status ClientBase::CreateData(const json& tree, ObjectID& id,
                              Signature& signature, InstanceID& instance_id) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteCreateDataRequest(tree, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadCreateDataReply(message_in, id, signature, instance_id));
  return Status::OK();
}

Status ClientBase::CreateMetaData(ObjectMeta& meta_data, ObjectID& id) {
  InstanceID instance_id = this->instance_id_;
  meta_data.SetInstanceId(instance_id);
  meta_data.AddKeyValue("transient", true);
  // nbytes is optional
  if (!meta_data.Haskey("nbytes")) {
    meta_data.SetNBytes(0);
  }
  // if the metadata has incomplete components, trigger an remote meta sync.
  if (meta_data.incomplete()) {
    json dummy;
    VINEYARD_SUPPRESS(GetData(InvalidObjectID(), dummy, true, false));
  }
  Signature signature;
  auto status = CreateData(meta_data.MetaData(), id, signature, instance_id);
  if (status.ok()) {
    meta_data.SetId(id);
    meta_data.SetSignature(signature);
    meta_data.SetClient(this);
    meta_data.SetInstanceId(instance_id);
    if (meta_data.incomplete()) {
      RETURN_ON_ERROR(this->GetMetaData(id, meta_data));
      meta_data.incomplete_ = false;
    }
  }
  return status;
}

Status ClientBase::DelData(const ObjectID id, const bool force,
                           const bool deep) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteDelDataRequest(id, force, deep, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadDelDataReply(message_in));
  return Status::OK();
}

Status ClientBase::DelData(const std::vector<ObjectID>& ids, const bool force,
                           const bool deep) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteDelDataRequest(ids, force, deep, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadDelDataReply(message_in));
  return Status::OK();
}

Status ClientBase::ListData(std::string const& pattern, bool const regex,
                            size_t const limit,
                            std::unordered_map<ObjectID, json>& meta_trees) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteListDataRequest(pattern, regex, limit, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadGetDataReply(message_in, meta_trees));
  return Status::OK();
}

Status ClientBase::Persist(const ObjectID id) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WritePersistRequest(id, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadPersistReply(message_in));
  return Status::OK();
}

Status ClientBase::IfPersist(const ObjectID id, bool& persist) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteIfPersistRequest(id, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadIfPersistReply(message_in, persist));
  return Status::OK();
}

Status ClientBase::Exists(const ObjectID id, bool& exists) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteExistsRequest(id, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadExistsReply(message_in, exists));
  return Status::OK();
}

Status ClientBase::ShallowCopy(const ObjectID id, ObjectID& target_id) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteShallowCopyRequest(id, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadShallowCopyReply(message_in, target_id));
  return Status::OK();
}

Status ClientBase::PutName(const ObjectID id, std::string const& name) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WritePutNameRequest(id, name, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadPutNameReply(message_in));
  return Status::OK();
}

Status ClientBase::GetName(const std::string& name, ObjectID& id,
                           const bool wait) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteGetNameRequest(name, wait, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadGetNameReply(message_in, id));
  return Status::OK();
}

Status ClientBase::DropName(const std::string& name) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteDropNameRequest(name, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadDropNameReply(message_in));
  return Status::OK();
}

Status ClientBase::MigrateObject(const ObjectID object_id,
                                 ObjectID& result_id) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteMigrateObjectRequest(object_id, message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  RETURN_ON_ERROR(ReadMigrateObjectReply(message_in, result_id));
  return Status::OK();
}

bool ClientBase::Connected() const {
  if (connected_ &&
      recv(vineyard_conn_, NULL, 1, MSG_PEEK | MSG_DONTWAIT) != -1) {
    connected_ = false;
  }
  return connected_;
}

void ClientBase::Disconnect() {
  std::lock_guard<std::recursive_mutex> __guard(this->client_mutex_);
  if (!this->connected_) {
    return;
  }
  std::string message_out;
  WriteExitRequest(message_out);
  VINEYARD_SUPPRESS(doWrite(message_out));
  close(vineyard_conn_);
  connected_ = false;
}

Status ClientBase::doWrite(const std::string& message_out) {
  auto status = send_message(vineyard_conn_, message_out);
  if (!status.ok()) {
    connected_ = false;
  }
  return status;
}

Status ClientBase::doRead(std::string& message_in) {
  return recv_message(vineyard_conn_, message_in);
}

Status ClientBase::doRead(json& root) {
  std::string message_in;
  auto status = recv_message(vineyard_conn_, message_in);
  if (!status.ok()) {
    connected_ = false;
    return status;
  }
  status = CATCH_JSON_ERROR([&]() -> Status {
    root = json::parse(message_in);
    return Status::OK();
  }());
  if (!status.ok()) {
    connected_ = false;
  }
  return status;
}

Status ClientBase::ClusterInfo(std::map<InstanceID, json>& meta) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteClusterMetaRequest(message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  json cluster_meta;
  RETURN_ON_ERROR(ReadClusterMetaReply(message_in, cluster_meta));
  for (auto& kv : json::iterator_wrapper(cluster_meta)) {
    InstanceID instance_id = UnspecifiedInstanceID();
    std::stringstream(kv.key().substr(1)) >> instance_id;
    meta.emplace(instance_id, kv.value());
  }
  return Status::OK();
}

Status ClientBase::InstanceStatus(
    std::shared_ptr<struct InstanceStatus>& status) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteInstanceStatusRequest(message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  json status_json;
  RETURN_ON_ERROR(ReadInstanceStatusReply(message_in, status_json));
  status.reset(new struct InstanceStatus(status_json));
  return Status::OK();
}

Status ClientBase::Instances(std::vector<InstanceID>& instances) {
  ENSURE_CONNECTED(this);
  std::string message_out;
  WriteClusterMetaRequest(message_out);
  RETURN_ON_ERROR(doWrite(message_out));
  json message_in;
  RETURN_ON_ERROR(doRead(message_in));
  json cluster_meta;
  RETURN_ON_ERROR(ReadClusterMetaReply(message_in, cluster_meta));
  for (auto& kv : json::iterator_wrapper(cluster_meta)) {
    InstanceID instance_id;
    std::stringstream(kv.key().substr(1)) >> instance_id;
    instances.emplace_back(instance_id);
  }
  return Status::OK();
}

InstanceStatus::InstanceStatus(const json& tree)
    : instance_id(tree["instance_id"].get<InstanceID>()),
      deployment(tree["deployment"].get_ref<const std::string&>()),
      memory_usage(tree["memory_usage"].get<size_t>()),
      memory_limit(tree["memory_limit"].get<size_t>()),
      deferred_requests(tree["deferred_requests"].get<size_t>()),
      ipc_connections(tree["ipc_connections"].get<size_t>()),
      rpc_connections(tree["rpc_connections"].get<size_t>()) {}

}  // namespace vineyard
