/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "rpc_tracker.h"

#include <dmlc/json.h>
#include <tvm/runtime/registry.h>
#include <tvm/support/logging.h>

#include <iomanip>
#include <iostream>
#include <memory>

namespace tvm {
namespace runtime {
namespace rpc {

std::unique_ptr<RPCTracker> RPCTracker::rpc_tracker_ = nullptr;

int RPCTrackerStart(std::string host, int port, int port_end, bool silent) {
  return RPCTracker::Start(host, port, port_end, silent);
}

void RPCTrackerStop() {
  std::cout << __FILE__ << " " << __LINE__ << " RPCTrackerStop" << std::endl;
  RPCTracker* tracker = RPCTracker::GetTracker();
  tracker->Stop();
}

void RPCTrackerTerminate() {
  std::cout << __FILE__ << " " << __LINE__ << " RPCTrackerTerminate" << std::endl;
  RPCTracker* tracker = RPCTracker::GetTracker();
  tracker->Terminate();
}


RPCTracker::RPCTracker(std::string host, int port, int port_end, bool silent)
    : host_{host}, port_{port}, port_end_{port_end}, silent_{silent} {
  listen_sock_.Create();
  my_port_ = listen_sock_.TryBindHost(host_, port_, port_end_);
  LOG(INFO) << "bind to " << host_ << ":" << my_port_;
  listen_sock_.Listen(1);
  listener_task_ = std::make_unique<std::thread>(&RPCTracker::ListenLoopEntry, this);
  listener_task_->detach();
}

RPCTracker::~RPCTracker() {
  std::cout << __FILE__ << " " << __LINE__ << std::endl;
  std::cout << __FILE__ << " " << __LINE__ << std::endl;
  if (listener_task_->joinable()) {
    std::cout << __FILE__ << " " << __LINE__ << std::endl;
  }
  listener_task_ = nullptr;
  }

RPCTracker* RPCTracker::GetTracker() { return rpc_tracker_.get(); }

int RPCTracker::GetPort() const { return my_port_; }

int RPCTracker::Start(std::string host, int port, int port_end, bool silent) {
  RPCTracker* tracker = RPCTracker::GetTracker();
  int result = -1;
  if (!tracker) {
    rpc_tracker_ = std::make_unique<RPCTracker>(host, port, port_end, silent);
  }
  result = rpc_tracker_->GetPort();
  return result;
}

void RPCTracker::Stop() {
  std::cout << __FILE__ << " " << __LINE__ << " RPCTracker::Stop" << std::endl;
  // For now call Terminate
  Terminate();
}

void RPCTracker::Terminate() {
  std::cout << __FILE__ << " " << __LINE__ << " RPCTracker::Terminate" << std::endl;
  // Delete the RPCTracker object to terminate
  rpc_tracker_ = nullptr;
}

/*!
 * \brief ListenLoopProc The listen process.
 */
void RPCTracker::ListenLoopEntry() {
  while (true) {
    support::TCPSocket connection = listen_sock_.Accept();
    // connection.SetNonBlock(false);
    std::string peer_host;
    int peer_port;
    connection.GetPeerAddress(peer_host, peer_port);
    std::lock_guard<std::mutex> guard(mutex_);
    connection_list_.insert(
        std::make_shared<ConnectionInfo>(this, peer_host, peer_port, connection));
  }
}

void RPCTracker::Put(std::string key, std::string address, int port, std::string match_key,
             std::shared_ptr<ConnectionInfo> conn) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (scheduler_map_.find(key) == scheduler_map_.end()) {
    // There is no scheduler for this key yet so add one
    scheduler_map_.insert({key, std::make_shared<PriorityScheduler>(key)});
  }
  auto it = scheduler_map_.find(key);
  if (it != scheduler_map_.end()) {
    it->second->Put(address, port, match_key, conn);
  } else {
    std::cout << __FILE__ << " " << __LINE__ << " put error" << key << std::endl;
    for (auto p : scheduler_map_) {
      std::cout << __FILE__ << " " << __LINE__ << " " << p.first << std::endl;
    }
  }
}

void RPCTracker::Request(std::string key, std::string user, int priority,
                         std::shared_ptr<ConnectionInfo> conn) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (scheduler_map_.find(key) == scheduler_map_.end()) {
    // There is no scheduler for this key yet so add one
    scheduler_map_.insert({key, std::make_shared<PriorityScheduler>(key)});
  }
  auto it = scheduler_map_.find(key);
  if (it != scheduler_map_.end()) {
    it->second->Request(user, priority, conn);
  } else {
    std::cout << __FILE__ << " " << __LINE__ << " request error" << key << std::endl;
    for (auto p : scheduler_map_) {
      std::cout << __FILE__ << " " << __LINE__ << " " << p.first << std::endl;
    }
  }
}

std::string RPCTracker::Summary() {
  std::stringstream ss;
  int count = scheduler_map_.size();
  for (auto p : scheduler_map_) {
    ss << "\"" << p.first << "\": " << p.second->Summary();
    if (--count > 0) {
      ss << ", ";
    }
  }
  return ss.str();
}

void RPCTracker::Close(std::shared_ptr<ConnectionInfo> conn) {
  std::lock_guard<std::mutex> guard(mutex_);
  connection_list_.erase(conn);
  std::string key = conn->key_;
  if (!key.empty()) {
    // "server:rasp3b" -> "rasp3b"
    auto pos = key.find(':');
    if (pos != std::string::npos) {
      key = key.substr(pos+1);
    }
    // TODO: rkimball remove values from scheduler_map
  }
}

void RPCTracker::ConnectionInfo::SendResponse(TRACKER_CODE value) {
  std::stringstream ss;
  ss << static_cast<int>(value);
  std::string status = ss.str();
  SendStatus(status);
}

void RPCTracker::ConnectionInfo::SendStatus(std::string status) {
  int length = status.size();

  connection_.SendAll(&length, sizeof(length));
  std::cout << host_ << ":" << port_ << " << " << status << std::endl;
  connection_.SendAll(status.data(), status.size());
}

RPCTracker::PriorityScheduler::PriorityScheduler(std::string key) : key_{key} {}

void RPCTracker::PriorityScheduler::Request(std::string user, int priority,
                                            std::shared_ptr<ConnectionInfo> conn) {
  std::lock_guard<std::mutex> guard(mutex_);
  requests_.emplace_back(user, priority, request_count_++, conn);
  std::sort(requests_.begin(), requests_.end(),
            [](const RPCTracker::RequestInfo& a, const RPCTracker::RequestInfo& b) {
              return a.priority_ > b.priority_;
            });
  Schedule();
}

void RPCTracker::PriorityScheduler::Put(std::string address, int port, std::string match_key,
             std::shared_ptr<ConnectionInfo> conn) {
  std::lock_guard<std::mutex> guard(mutex_);
  values_.emplace_back(address, port, match_key, conn);
  Schedule();
}

void RPCTracker::PriorityScheduler::Remove(PutInfo value) {
  std::lock_guard<std::mutex> guard(mutex_);
  auto it = std::find(values_.begin(), values_.end(), value);
  if (it != values_.end()) {
    values_.erase(it);
    Schedule();
  }
}

std::string RPCTracker::PriorityScheduler::Summary() {
  std::stringstream ss;
  ss << "{\"free\": " << values_.size() << ", \"pending\": " << requests_.size() << "}";
  return ss.str();
}

void RPCTracker::PriorityScheduler::Schedule() {
  while (!requests_.empty() && !values_.empty()) {
    PutInfo& pi = values_[0];
    RequestInfo& request = requests_[0];
    try {
      std::stringstream ss;
      ss << "[" << static_cast<int>(TRACKER_CODE::SUCCESS) << ", [\"" << pi.address_ << "\", "
         << pi.port_ << ", \"" << pi.match_key_ << "\"]]";
      request.conn_->SendStatus(ss.str());
      pi.conn_->pending_match_keys_.erase(pi.match_key_);
    } catch (...) {
      values_.push_back(pi);
    }

    values_.pop_front();
    requests_.pop_front();
  }
}

RPCTracker::ConnectionInfo::ConnectionInfo(RPCTracker* tracker, std::string host, int port,
                                           support::TCPSocket connection)
    : tracker_{tracker}, host_{host}, port_{port}, connection_{connection} {
  connection_task_ =
      std::thread(&RPCTracker::ConnectionInfo::ConnectionLoop, this);
  connection_task_.detach();
}

void RPCTracker::ConnectionInfo::Close() {
  std::cout << __FILE__ << " " << __LINE__ << " ****************** RPCTracker::ConnectionInfo::Close\n";
  connection_.Close();
}

int RPCTracker::ConnectionInfo::RecvAll(void* data, size_t length) {
  char* buf = static_cast<char*>(data);
  size_t remainder = length;
  while (remainder > 0) {
    int read_length = connection_.Recv(buf, remainder);
    if (read_length <= 0) {
      return -1;
    }
    remainder -= read_length;
    buf += read_length;
  }
  return length;
}

void RPCTracker::ConnectionInfo::ConnectionLoop() {
  // Do magic handshake
  int magic = 0;
  if (RecvAll(&magic, sizeof(magic)) == -1) {
    // Error setting up connection
    return;
  }
  if (magic != static_cast<int>(RPC_CODE::RPC_TRACKER_MAGIC)) {
    // Not a tracker connection so close connection and exit
    return;
  }
  connection_.SendAll(&magic, sizeof(magic));

  while (true) {
    std::string json;
    bool fail = false;
    try {
      int length = 0;
      if (RecvAll(&length, sizeof(length)) != sizeof(length)) {
        fail = true;
      }
      json.resize(length);
      if(!fail && RecvAll(&json[0], length) != length) {
        fail = true;
      }
    } catch (std::exception err) {
      fail = true;
      // This means that the connection has gone down. Tell the tracker to remove it.
    }

    if (fail) {
      auto tmp_p = shared_from_this();
      tracker_->Close(tmp_p);
      Close();
      return;
    }

    std::cout << host_ << ":" << port_ << " >> " << json << std::endl;

    std::istringstream is(json);
    dmlc::JSONReader reader(&is);
    int tmp;
    reader.BeginArray();
    reader.NextArrayItem();
    reader.ReadNumber(&tmp);
    reader.NextArrayItem();
    switch (static_cast<TRACKER_CODE>(tmp)) {
      case TRACKER_CODE::FAIL:
        break;
      case TRACKER_CODE::SUCCESS:
        break;
      case TRACKER_CODE::PING:
        SendResponse(TRACKER_CODE::SUCCESS);
        break;
      case TRACKER_CODE::STOP:
        SendResponse(TRACKER_CODE::SUCCESS);
        tracker_->Stop();
        break;
      case TRACKER_CODE::PUT: {
        std::string key;
        int port;
        std::string match_key;
        std::string addr = host_;
        reader.Read(&key);
        reader.NextArrayItem();
        reader.BeginArray();
        reader.NextArrayItem();
        reader.Read(&port);
        reader.NextArrayItem();
        reader.Read(&match_key);
        reader.NextArrayItem();  // This is an EndArray
        if (reader.NextArrayItem()) {
          // 4 args in message
          std::string tmp;
          try {
            reader.Read(&tmp);
          } catch (...) {
            // Not a string so we don't care
          }
          if (!tmp.empty() && tmp != "null") {
            addr = tmp;
          }
        }
        pending_match_keys_.insert(match_key);
        // auto put_info = std::make_shared<PutInfo>(addr, port, match_key, shared_from_this());
        tracker_->Put(key, addr, port, match_key, shared_from_this());
        // put_values_.insert(put_info);
        SendResponse(TRACKER_CODE::SUCCESS);
        break;
      }
      case TRACKER_CODE::REQUEST: {
        std::string key;
        std::string user;
        int priority;
        reader.Read(&key);
        reader.NextArrayItem();
        reader.Read(&user);
        reader.NextArrayItem();
        reader.Read(&priority);
        reader.NextArrayItem();
        tracker_->Request(key, user, priority, shared_from_this());
        break;
      }
      case TRACKER_CODE::UPDATE_INFO: {
        std::string key;
        std::string value;
        reader.BeginObject();
        reader.NextObjectItem(&key);
        reader.Read(&value);
        key_ = value;
        SendResponse(TRACKER_CODE::SUCCESS);
        break;
      }
      case TRACKER_CODE::SUMMARY: {
        std::stringstream ss;
        ss << "[" << static_cast<int>(TRACKER_CODE::SUCCESS) << ", {\"queue_info\": {"
           << tracker_->Summary() << "}, ";
        ss << "\"server_info\": [";
        int count = 0;
        {
          std::lock_guard<std::mutex> guard(tracker_->mutex_);
          for (auto conn : tracker_->connection_list_) {
            if (conn->key_.substr(0, 6) == "server") {
              if (count++ > 0) {
                ss << ", ";
              }
              ss << "{\"addr\": [\"" << conn->host_ << "\", " << conn->port_ << "], \"key\": \""
                << conn->key_ << "\"}";
            }
          }
        }
        ss << "]}]";
        SendStatus(ss.str());
        break;
      }
      case TRACKER_CODE::GET_PENDING_MATCHKEYS:
        std::stringstream ss;
        ss << "[";
        int count = 0;
        for (auto match_key : pending_match_keys_) {
          if (count++ > 0) {
            ss << ", ";
          }
          ss << "\"" << match_key << "\"";
        }
        ss << "]";
        SendStatus(ss.str());
        break;
    }
  }
}

}  // namespace rpc
TVM_REGISTER_GLOBAL("rpc.RPCTrackerStart").set_body_typed(tvm::runtime::rpc::RPCTrackerStart);
TVM_REGISTER_GLOBAL("rpc.RPCTrackerStop").set_body_typed(tvm::runtime::rpc::RPCTrackerStop);
TVM_REGISTER_GLOBAL("rpc.RPCTrackerTerminate").set_body_typed(tvm::runtime::rpc::RPCTrackerTerminate);
}  // namespace runtime
}  // namespace tvm