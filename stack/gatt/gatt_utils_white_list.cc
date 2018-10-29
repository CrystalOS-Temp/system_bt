/******************************************************************************
 *
 *  Copyright 2018 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "gatt_int.h"

#include <base/logging.h>

#include "btm_int.h"
#include "gatt_api.h"

/** Returns true if this is one of the background devices for the application,
 * false otherwise */
bool gatt_is_bg_dev_for_app(tGATT_BG_CONN_DEV* p_dev, tGATT_IF gatt_if) {
  return p_dev->gatt_if.count(gatt_if);
}

/** background connection device from the list. Returns pointer to the device
 * record, or nullptr if not found */
tGATT_BG_CONN_DEV* gatt_find_bg_dev(const RawAddress& remote_bda) {
  for (tGATT_BG_CONN_DEV& dev : gatt_cb.bgconn_dev) {
    if (dev.remote_bda == remote_bda) {
      return &dev;
    }
  }
  return nullptr;
}

std::list<tGATT_BG_CONN_DEV>::iterator gatt_find_bg_dev_it(
    const RawAddress& remote_bda) {
  auto& list = gatt_cb.bgconn_dev;
  for (auto it = list.begin(); it != list.end(); it++) {
    if (it->remote_bda == remote_bda) {
      return it;
    }
  }
  return list.end();
}

/** Add a device from the background connection list.  Returns true if device
 * added to the list, or already in list, false otherwise */
bool gatt_add_bg_dev_list(tGATT_REG* p_reg, const RawAddress& bd_addr) {
  tGATT_IF gatt_if = p_reg->gatt_if;

  tGATT_BG_CONN_DEV* p_dev = gatt_find_bg_dev(bd_addr);
  if (p_dev) {
    // device already in the whitelist, just add interested app to the list
    if (!p_dev->gatt_if.insert(gatt_if).second) {
      LOG(ERROR) << "device already in iniator white list";
    }

    return true;
  }
  // the device is not in the whitelist

  if (!BTM_WhiteListAdd(bd_addr)) return false;

  gatt_cb.bgconn_dev.emplace_back();
  tGATT_BG_CONN_DEV& dev = gatt_cb.bgconn_dev.back();
  dev.remote_bda = bd_addr;
  dev.gatt_if.insert(gatt_if);
  return true;
}

/** Removes all registrations for background connection for given device.
 * Returns true if anything was removed, false otherwise */
uint8_t gatt_clear_bg_dev_for_addr(const RawAddress& bd_addr) {
  auto dev_it = gatt_find_bg_dev_it(bd_addr);
  if (dev_it == gatt_cb.bgconn_dev.end()) return false;

  BTM_WhiteListRemove(dev_it->remote_bda);
  gatt_cb.bgconn_dev.erase(dev_it);
  return true;
}

/** Remove device from the background connection device list or listening to
 * advertising list.  Returns true if device was on the list and was succesfully
 * removed */
bool gatt_remove_bg_dev_from_list(tGATT_REG* p_reg, const RawAddress& bd_addr) {
  tGATT_IF gatt_if = p_reg->gatt_if;
  auto dev_it = gatt_find_bg_dev_it(bd_addr);
  if (dev_it == gatt_cb.bgconn_dev.end()) return false;

  if (!dev_it->gatt_if.erase(gatt_if)) return false;

  if (!dev_it->gatt_if.empty()) return true;

  // no more apps interested - remove from whitelist and delete record
  BTM_WhiteListRemove(dev_it->remote_bda);
  gatt_cb.bgconn_dev.erase(dev_it);
  return true;
}

/** deregister all related back ground connetion device. */
void gatt_deregister_bgdev_list(tGATT_IF gatt_if) {
  auto it = gatt_cb.bgconn_dev.begin();
  auto end = gatt_cb.bgconn_dev.end();
  /* update the BG conn device list */
  while (it != end) {
    it->gatt_if.erase(gatt_if);
    if (it->gatt_if.size()) {
      it++;
      continue;
    }

    BTM_WhiteListRemove(it->remote_bda);
    it = gatt_cb.bgconn_dev.erase(it);
  }
}

/** Reset bg device list. If called after controller reset, set |after_reset| to
 * true, as there is no need to wipe controller white list in this case. */
void gatt_reset_bgdev_list(bool after_reset) {
  gatt_cb.bgconn_dev.clear();
  if (!after_reset) BTM_WhiteListClear();
}
