// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <memory>
#include <lvgl.h>
namespace companion::muse {
class LocalPort;
// Calls must run on the LVGL owner thread with the LVGL lock held.
class LocalSetupUi {
 struct Impl; std::unique_ptr<Impl> impl_;
public:
 explicit LocalSetupUi(LocalPort&);
 ~LocalSetupUi();
 LocalSetupUi(const LocalSetupUi&)=delete;
 void Open(lv_obj_t* parent);
 void Poll();
 void Close();
 bool Visible() const;
};
}
