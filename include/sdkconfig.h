#pragma once

// The prebuilt Arduino sdkconfig.h still enables WPA-Enterprise and BLE
// provisioning. This firmware joins a personal network with a password.
// Search continues to the framework header.
#include_next "sdkconfig.h"

#undef CONFIG_NETWORK_PROV_NETWORK_TYPE_WIFI
#undef CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT
