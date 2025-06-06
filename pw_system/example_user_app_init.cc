// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#define PW_LOG_MODULE_NAME "user_init"

#include "pw_log/log.h"
#include "pw_rpc/echo_service_pwpb.h"
#include "pw_system/rpc_server.h"
#include "pw_trace/trace.h"

namespace pw::system {

pw::rpc::EchoService echo_service;

// This will run once after pw::system::Init() completes. This callback must
// return or it will block the work queue.
void UserAppInit() {
  PW_TRACE_FUNCTION();

  PW_LOG_INFO("Pigweed is fun!");
  GetRpcServer().RegisterService(echo_service);
}

}  // namespace pw::system
