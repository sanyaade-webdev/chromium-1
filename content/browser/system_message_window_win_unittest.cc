// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/system_message_window_win.h"

#include <dbt.h>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_string_conversions.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/mock_devices_changed_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

LRESULT GetVolumeName(LPCWSTR drive,
                      LPWSTR volume_name,
                      unsigned int volume_name_length) {
  DCHECK(volume_name_length > wcslen(drive) + 2);
  *volume_name = 'V';
  wcscpy(volume_name + 1, drive);
  return TRUE;
}

}  // namespace

class SystemMessageWindowWinTest : public testing::Test {
 public:
  SystemMessageWindowWinTest() : window_(&GetVolumeName) { }
  virtual ~SystemMessageWindowWinTest() { }

 protected:
  virtual void SetUp() OVERRIDE {
    system_monitor_.AddDevicesChangedObserver(&observer_);
  }

  void DoDevicesAttachedTest(const std::vector<int>& deviceIndices);
  void DoDevicesDetachedTest(const std::vector<int>& deviceIndices);

  MessageLoop message_loop_;
  base::SystemMonitor system_monitor_;
  base::MockDevicesChangedObserver observer_;
  SystemMessageWindowWin window_;
};

void SystemMessageWindowWinTest::DoDevicesAttachedTest(
    const std::vector<int>& device_indices) {
  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_unitmask = 0x0;
  volume_broadcast.dbcv_flags = 0x0;
  {
    testing::InSequence sequnce;
    for (std::vector<int>::const_iterator it = device_indices.begin();
         it != device_indices.end();
         ++it) {
      volume_broadcast.dbcv_unitmask |= 0x1 << *it;
      std::wstring drive(L"_:\\");
      drive[0] = 'A' + *it;
      std::string name("V");
      name.append(base::SysWideToUTF8(drive));
      EXPECT_CALL(observer_, OnMediaDeviceAttached(*it, name, FilePath(drive)));
    }
  }
  window_.OnDeviceChange(DBT_DEVICEARRIVAL,
                         reinterpret_cast<DWORD>(&volume_broadcast));
  message_loop_.RunAllPending();
};

void SystemMessageWindowWinTest::DoDevicesDetachedTest(
    const std::vector<int>& device_indices) {
  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_unitmask = 0x0;
  volume_broadcast.dbcv_flags = 0x0;
  {
    testing::InSequence sequence;
    for (std::vector<int>::const_iterator it = device_indices.begin();
         it != device_indices.end();
         ++it) {
      volume_broadcast.dbcv_unitmask |= 0x1 << *it;
      EXPECT_CALL(observer_, OnMediaDeviceDetached(*it));
    }
  }
  window_.OnDeviceChange(DBT_DEVICEREMOVECOMPLETE,
                         reinterpret_cast<DWORD>(&volume_broadcast));
  message_loop_.RunAllPending();
};

TEST_F(SystemMessageWindowWinTest, DevicesChanged) {
  EXPECT_CALL(observer_, OnDevicesChanged()).Times(1);
  window_.OnDeviceChange(DBT_DEVNODES_CHANGED, NULL);
  message_loop_.RunAllPending();
}

TEST_F(SystemMessageWindowWinTest, RandomMessage) {
  window_.OnDeviceChange(DBT_DEVICEQUERYREMOVE, NULL);
  message_loop_.RunAllPending();
}

TEST_F(SystemMessageWindowWinTest, DevicesAttached) {
  std::vector<int> device_indices;
  device_indices.push_back(1);
  device_indices.push_back(5);
  device_indices.push_back(7);

  DoDevicesAttachedTest(device_indices);
}

TEST_F(SystemMessageWindowWinTest, DevicesAttachedHighBoundary) {
  std::vector<int> device_indices;
  device_indices.push_back(25);

  DoDevicesAttachedTest(device_indices);
}

TEST_F(SystemMessageWindowWinTest, DevicesAttachedLowBoundary) {
  std::vector<int> device_indices;
  device_indices.push_back(0);

  DoDevicesAttachedTest(device_indices);
}

TEST_F(SystemMessageWindowWinTest, DevicesAttachedAdjacentBits) {
  std::vector<int> device_indices;
  device_indices.push_back(0);
  device_indices.push_back(1);
  device_indices.push_back(2);
  device_indices.push_back(3);

  DoDevicesAttachedTest(device_indices);
}

TEST_F(SystemMessageWindowWinTest, DevicesDetached) {
  std::vector<int> device_indices;
  device_indices.push_back(1);
  device_indices.push_back(5);
  device_indices.push_back(7);

  DoDevicesDetachedTest(device_indices);
}

TEST_F(SystemMessageWindowWinTest, DevicesDetachedHighBoundary) {
  std::vector<int> device_indices;
  device_indices.push_back(25);

  DoDevicesDetachedTest(device_indices);
}

TEST_F(SystemMessageWindowWinTest, DevicesDetachedLowBoundary) {
  std::vector<int> device_indices;
  device_indices.push_back(0);

  DoDevicesDetachedTest(device_indices);
}

TEST_F(SystemMessageWindowWinTest, DevicesDetachedAdjacentBits) {
  std::vector<int> device_indices;
  device_indices.push_back(0);
  device_indices.push_back(1);
  device_indices.push_back(2);
  device_indices.push_back(3);

  DoDevicesDetachedTest(device_indices);
}
