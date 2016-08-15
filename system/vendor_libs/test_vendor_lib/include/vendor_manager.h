//
// Copyright 2015 The Android Open Source Project
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

#pragma once

#include "async_manager.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "dual_mode_controller.h"
#include "event_packet.h"
#include "hci/include/bt_vendor_lib.h"
#include "hci_transport.h"
#include "test_channel_transport.h"

#include <memory>

namespace test_vendor_lib {

// Contains the three core objects that make up the test vendor library: the
// HciTransport for communication, the HciHandler for processing commands, and
// the Controller for actual command implementations. The VendorManager shall
// operate as a global singleton and be used in bt_vendor.cc to perform vendor
// specific operations, via |vendor_callbacks_|, and to provide access to the
// test controller by setting up a message loop (on another thread) that the HCI
// will talk to and controller methods will execute on.
class VendorManager {
 public:
  // Functions that operate on the global manager instance. Initialize()
  // is called by the vendor library's TestVendorInitialize() function to create
  // the global manager and must be called before Get() and CleanUp().
  // CleanUp() should be called when a call to TestVendorCleanUp() is made
  // since the global manager should live throughout the entire time the test
  // vendor library is in use.
  static void CleanUp();

  static VendorManager* Get();

  static void Initialize();

  void CloseHciFd();

  int GetHciFd() const;

  const bt_vendor_callbacks_t& GetVendorCallbacks() const;

  // Stores a copy of the vendor specific configuration callbacks passed into
  // the vendor library from the HCI in TestVendorInit().
  void SetVendorCallbacks(const bt_vendor_callbacks_t& callbacks);

  // Returns true if |thread_| is able to be started and the
  // StartingWatchingOnThread() task has been posted to the task runner.
  bool Run();

 private:
  VendorManager();

  ~VendorManager() = default;

  // Starts watching for incoming data from the HCI and the test hook.
  void StartWatchingOnThread();

  // Creates the HCI's communication channel and overrides IO callbacks to
  // receive and send packets.
  HciTransport transport_;

  // The controller object that provides implementations of Bluetooth commands.
  DualModeController controller_;

  // The two test channel objects that perform functions corresponding to the
  // HciTransport and HciHandler.
  TestChannelTransport test_channel_transport_;

  // Configuration callbacks provided by the HCI for use in TestVendorOp().
  bt_vendor_callbacks_t vendor_callbacks_;

  // True if the underlying message loop (in |thread_|) is running.
  bool running_;

  // The object that manages asynchronous tasks such as watching a file
  // descriptor or doing something in the future
  AsyncManager async_manager_;

  VendorManager(const VendorManager& cmdPckt) = delete;
  VendorManager& operator=(const VendorManager& cmdPckt) = delete;
};

}  // namespace test_vendor_lib
