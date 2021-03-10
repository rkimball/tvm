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

#include <iostream>
#include <future>
#include <memory>

#include "rpc_tracker.h"

#include <tvm/runtime/registry.h>

namespace tvm {
namespace runtime {
namespace rpc {
std::unique_ptr<RPCTracker> RPCTracker::rpc_tracker_;

int RPCTrackerEntry(std::string host, int port, int port_end, bool silent) {
  std::cout << __FILE__ << " " << __LINE__ << std::endl;
  int result = -1;
  RPCTracker* tracker = RPCTracker::GetTracker();
  if (!tracker) {
    // Tracker is not currently running so start it
    result = RPCTracker::Start(host, port, port_end, silent);
  }
  return result;
}

RPCTracker::RPCTracker(std::string host, int port, int port_end, bool silent)
: host_{host}, port_{port}, port_end_{port_end}, silent_{silent} {
  std::cout << __FILE__ << " " << __LINE__ << std::endl;
  listen_sock_.Create();
  my_port_ = listen_sock_.TryBindHost(host_, port_, port_end_);
  LOG(INFO) << "bind to " << host_ << ":" << my_port_;
  listen_sock_.Listen(1);
  std::future<void> proc(std::async(std::launch::async, &RPCTracker::ListenLoopProc, this));
  // proc.get();
  // // Close the listen socket
  // listen_sock_.Close();
}

RPCTracker::~RPCTracker() {
  std::cout << __FILE__ << " " << __LINE__ << std::endl;
  std::cout << host_ << std::endl;
  std::cout << port_ << std::endl;
  std::cout << port_end_ << std::endl;
  std::cout << silent_ << std::endl;
}

RPCTracker* RPCTracker::GetTracker(){
  return rpc_tracker_.get();
}

int RPCTracker::GetPort() const { return my_port_; }

int RPCTracker::Start(std::string host, int port, int port_end, bool silent) {
  RPCTracker* tracker = RPCTracker::GetTracker();
  int result = -1;
  if (!tracker) {
    rpc_tracker_ = std::make_unique<RPCTracker>(host, port, port_end, silent);
    result = rpc_tracker_->GetPort();
  }

  return result;

}

/*!
 * \brief ListenLoopProc The listen process.
 */
void RPCTracker::ListenLoopProc() {
  std::cout << __FILE__ << " " << __LINE__ << std::endl;
//   TrackerClient tracker(tracker_addr_, key_, custom_addr_);
//   while (true) {
//     support::TCPSocket conn;
//     support::SockAddr addr("0.0.0.0", 0);
//     std::string opts;
//     try {
//       // step 1: setup tracker and report to tracker
//       tracker.TryConnect();
//       // step 2: wait for in-coming connections
//       AcceptConnection(&tracker, &conn, &addr, &opts);
//     } catch (const char* msg) {
//       LOG(WARNING) << "Socket exception: " << msg;
//       // close tracker resource
//       tracker.Close();
//       continue;
//     } catch (const std::exception& e) {
//       // close tracker resource
//       tracker.Close();
//       LOG(WARNING) << "Exception standard: " << e.what();
//       continue;
//     }

//     int timeout = GetTimeOutFromOpts(opts);
// #if defined(__linux__) || defined(__ANDROID__)
//     // step 3: serving
//     if (timeout != 0) {
//       const pid_t timer_pid = fork();
//       if (timer_pid == 0) {
//         // Timer process
//         sleep(timeout);
//         exit(0);
//       }

//       const pid_t worker_pid = fork();
//       if (worker_pid == 0) {
//         // Worker process
//         ServerLoopProc(conn, addr);
//         exit(0);
//       }

//       int status = 0;
//       const pid_t finished_first = waitPidEintr(&status);
//       if (finished_first == timer_pid) {
//         kill(worker_pid, SIGTERM);
//       } else if (finished_first == worker_pid) {
//         kill(timer_pid, SIGTERM);
//       } else {
//         LOG(INFO) << "Child pid=" << finished_first << " unexpected, but still continue.";
//       }

//       int status_second = 0;
//       waitPidEintr(&status_second);

//       // Logging.
//       if (finished_first == timer_pid) {
//         LOG(INFO) << "Child pid=" << worker_pid << " killed (timeout = " << timeout
//                   << "), Process status = " << status_second;
//       } else if (finished_first == worker_pid) {
//         LOG(INFO) << "Child pid=" << timer_pid << " killed, Process status = " << status_second;
//       }
//     } else {
//       auto pid = fork();
//       if (pid == 0) {
//         ServerLoopProc(conn, addr);
//         exit(0);
//       }
//       // Wait for the result
//       int status = 0;
//       wait(&status);
//       LOG(INFO) << "Child pid=" << pid << " exited, Process status =" << status;
//     }
// #elif defined(WIN32)
//     auto start_time = high_resolution_clock::now();
//     try {
//       SpawnRPCChild(conn.sockfd, seconds(timeout));
//     } catch (const std::exception&) {
//     }
//     auto dur = high_resolution_clock::now() - start_time;

//     LOG(INFO) << "Serve Time " << duration_cast<milliseconds>(dur).count() << "ms";
// #endif
//     // close from our side.
//     LOG(INFO) << "Socket Connection Closed";
//     conn.Close();
//   }
}

}  // namespace rpc
TVM_REGISTER_GLOBAL("rpc.RPCTrackerStart").set_body_typed(tvm::runtime::rpc::RPCTrackerEntry);
}  // namespace runtime
}  // namespace tvm
