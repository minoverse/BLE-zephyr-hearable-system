# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

# 1. Version Info
set(APP_VERSION_MAJOR 1)
set(APP_VERSION_MINOR 0)
set(APP_VERSION_PATCH 0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# 2. Project Name
project(ble_hearable VERSION ${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH})

# 3. Add source files
target_sources(app PRIVATE
  src/main.c
  src/imu.c
  src/gesture_detect.c
  src/gesture_thread.c
  src/adaptive_power.c
  src/power_stats.c
  src/watchdog.c
)

if(CONFIG_BT)
  target_sources(app PRIVATE src/gesture_service.c src/fault_recovery.c src/ble_adaptive.c src/network_stress.c src/state_reporter.c)
endif()

# 4. Pass versions to C code
target_compile_definitions(app PRIVATE
  -DAPP_VERSION_MAJOR=${APP_VERSION_MAJOR}
  -DAPP_VERSION_MINOR=${APP_VERSION_MINOR}
  -DAPP_VERSION_PATCH=${APP_VERSION_PATCH}
)
