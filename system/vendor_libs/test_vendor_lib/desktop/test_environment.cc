//
// Copyright 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "test_environment.h"

#include <type_traits>  // for remove_extent_t
#include <utility>      // for move
#include <vector>       // for vector

#include "net/async_data_channel.h"  // for AsyncDataChannel
#include "os/log.h"                  // for LOG_INFO, LOG_ERROR, LOG_WARN

namespace android {
namespace bluetooth {
namespace root_canal {

using test_vendor_lib::AsyncTaskId;
using test_vendor_lib::TaskCallback;

void TestEnvironment::initialize(std::promise<void> barrier) {
  LOG_INFO();

  barrier_ = std::move(barrier);

  auto user_id = async_manager_.GetNextUserId();
  test_channel_transport_.RegisterCommandHandler(
      [this, user_id](const std::string& name,
                      const std::vector<std::string>& args) {
        async_manager_.ExecAsync(user_id, std::chrono::milliseconds(0),
                                 [this, name, args]() {
                                   if (name == "END_SIMULATION") {
                                     barrier_.set_value();
                                   } else {
                                     test_channel_.HandleCommand(name, args);
                                   }
                                 });
      });

  SetUpTestChannel();
  SetUpHciServer([this](std::shared_ptr<AsyncDataChannel> socket,
                        AsyncDataChannelServer* srv) {
    test_model_.IncomingHciConnection(socket, controller_properties_file_);
    srv->StartListening();
  });
  SetUpLinkLayerServer();
  SetUpLinkBleLayerServer();

  LOG_INFO("%s: Finished", __func__);
}

void TestEnvironment::close() {
  LOG_INFO("%s", __func__);
  test_model_.Reset();
}

void TestEnvironment::SetUpHciServer(ConnectCallback connection_callback) {
  test_channel_.RegisterSendResponse([](const std::string& response) {
    LOG_INFO("No HCI Response channel: %s", response.c_str());
  });

  if (!remote_hci_transport_.SetUp(hci_socket_server_, connection_callback)) {
    LOG_ERROR("Remote HCI channel SetUp failed.");
    return;
  }
}

void TestEnvironment::SetUpLinkBleLayerServer() {
  remote_link_layer_transport_.SetUp(
      link_ble_socket_server_, [this](std::shared_ptr<AsyncDataChannel> socket,
                                      AsyncDataChannelServer* srv) {
        test_model_.IncomingLinkBleLayerConnection(socket);
        srv->StartListening();
      });

  test_channel_.RegisterSendResponse([](const std::string& response) {
    LOG_INFO("No LinkLayer Response channel: %s", response.c_str());
  });
}

void TestEnvironment::SetUpLinkLayerServer() {
  remote_link_layer_transport_.SetUp(
      link_socket_server_, [this](std::shared_ptr<AsyncDataChannel> socket,
                                  AsyncDataChannelServer* srv) {
        test_model_.IncomingLinkLayerConnection(socket);
        srv->StartListening();
      });

  test_channel_.RegisterSendResponse([](const std::string& response) {
    LOG_INFO("No LinkLayer Response channel: %s", response.c_str());
  });
}

std::shared_ptr<AsyncDataChannel> TestEnvironment::ConnectToRemoteServer(
    const std::string& server, int port) {
  return connector_->ConnectToRemoteServer(server, port);
}

void TestEnvironment::SetUpTestChannel() {
  bool transport_configured = test_channel_transport_.SetUp(
      test_socket_server_, [this](std::shared_ptr<AsyncDataChannel> conn_fd,
                                  AsyncDataChannelServer* server) {
        LOG_INFO("Test channel connection accepted.");
        server->StartListening();
        if (test_channel_open_) {
          LOG_WARN("Only one connection at a time is supported");
          test_channel_transport_.SendResponse(conn_fd,
                                               "The connection is broken");
          return false;
        }
        test_channel_open_ = true;
        test_channel_.RegisterSendResponse(
            [this, conn_fd](const std::string& response) {
              test_channel_transport_.SendResponse(conn_fd, response);
            });

        conn_fd->WatchForNonBlockingRead([this](AsyncDataChannel* conn_fd) {
          test_channel_transport_.OnCommandReady(
              conn_fd, [this]() { test_channel_open_ = false; });
        });
        return false;
      });
  test_channel_.RegisterSendResponse([](const std::string& response) {
    LOG_INFO("No test channel: %s", response.c_str());
  });
  test_channel_.AddPhy({"BR_EDR"});
  test_channel_.AddPhy({"LOW_ENERGY"});
  test_channel_.SetTimerPeriod({"5"});
  test_channel_.StartTimer({});

  test_channel_.FromFile(default_commands_file_);

  if (!transport_configured) {
    LOG_ERROR("Test channel SetUp failed.");
    return;
  }

  LOG_INFO("Test channel SetUp() successful");
}

}  // namespace root_canal
}  // namespace bluetooth
}  // namespace android
