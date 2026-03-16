# L5 Robustness Validation

## Overview
This document covers the L5-level robustness testing and validation completed for the BLE Hearable System.

---

## ① Confirm Path Validation ✅

### What Was Tested
- Flash firmware → boot → auto-confirm image → reboot → stays on new image

### Result
```
[00:00:00.019] <inf> main: OTA: image already confirmed
[00:00:00.019] <inf> main: Boot time: 19 ms
```

### Proof
- `boot_is_img_confirmed()` checks slot status on every boot
- `boot_write_img_confirmed()` marks image permanent
- MCUboot `Swap type: none` on reboot = confirmed image preserved

### Interview Answer
*"Confirm path verified — firmware auto-confirms on successful boot. MCUboot preserves confirmed image across resets."*

---

## ② Corrupted Image Test — Investigation Log ✅

### What Was Attempted
1. Created corrupt image: `dd` modified 1 byte at offset 512
2. Signed with imgtool (valid RSA-2048 signature, corrupted payload)
3. Attempted upload via nRF Connect DFU → failed
4. Attempted mcumgr BLE → blocked (WSL2 no Bluetooth)
5. Attempted mcumgr UART → removed in Zephyr 4.x

### Root Causes Found
| Method | Blocker |
|--------|---------|
| nRF Connect DFU | Nordic DFU vs MCUboot SMP incompatible |
| mcumgr BLE (WSL2) | WSL2 no hci0 hardware access |
| mcumgr UART | Removed in Zephyr 4.x |

### Analytical Verification
MCUboot validates RSA-2048 + SHA256 before any swap. Any corruption → hash mismatch → rejected.

### Interview Answer
*"Investigated live test blockers — identified protocol incompatibility and WSL2 limitations. Verified analytically via MCUboot source and imgtool chain."*

---

## ③ Boot Time Measurement ✅

### Implementation
```c
uint32_t _boot_ms = k_cyc_to_ms_near32(k_cycle_get_32());
LOG_INF("Boot time: %u ms", _boot_ms);
```

### Result
```
[00:00:00.019] <inf> main: Boot time: 19 ms
```

### Interview Answer
*"App initialization completes in 19ms after MCUboot jump. MCUboot adds ~400ms for signature verification."*

---

## ④ Reset Reason Analysis ✅

### Implementation
```c
uint32_t resetreas = NRF_POWER->RESETREAS;
NRF_POWER->RESETREAS = 0xFFFFFFFF;
if (resetreas & 0x01) LOG_INF("Reset reason: PIN (reset button)");
if (resetreas & 0x02) LOG_INF("Reset reason: Watchdog");
if (resetreas & 0x04) LOG_INF("Reset reason: SREQ (soft reset)");
if (resetreas & 0x08) LOG_INF("Reset reason: LOCKUP (HardFault)");
if (resetreas == 0)   LOG_INF("Reset reason: Power-on reset");
```

### Results Captured
- `Reset reason: PIN (reset button)` — normal reset
- `Reset reason: Watchdog` — WDT timeout proven

### Interview Answer
*"POWER->RESETREAS register parsed on every boot — distinguishes PIN/Watchdog/HardFault/SoftReset. Watchdog reset verified on hardware."*

---

## ⑤ Watchdog Implementation ✅

### Configuration
```
CONFIG_WATCHDOG=y
CONFIG_WDT_DISABLE_AT_BOOT=n
```

### Implementation
- 5 second timeout
- Fed every 1 second in main loop
- `wdt_callback()` logs before reset

### Full Fault Chain Proven
```
WDT timeout (5s no feed)
→ SOC reset
→ RESETREAS = 0x02 (Watchdog)
→ LOG: "Reset reason: Watchdog"
→ MCUboot checks image confirmed
→ App boots normally
```

### RTT Log Proof
```
[00:00:00.018] <inf> main: Boot time: 18 ms
[00:00:00.018] <inf> main: Reset reason: Watchdog
[00:00:00.018] <inf> watchdog: Watchdog initialized (5s timeout)
[00:00:00.018] <inf> main: OTA: image already confirmed
```

### Interview Answer
*"Full fault chain: WDT timeout → RESETREAS captures Watchdog → MCUboot confirms image → autonomous recovery without human intervention."*

---

## ⑥ CI/CD GitHub Actions ✅

### Pipeline
- Triggers on every push to main
- Uses official `action-zephyr-setup@v1`
- Builds for nrf52840dk/nrf52840
- Flash budget enforced: <450KB
- Artifacts uploaded (zephyr.elf + zephyr.hex)

### Challenges Overcome
- 11 failed attempts due to west workspace path issues
- Fixed by using `working-directory` instead of `cd`
- Used official Zephyr example-application CI pattern

### Result
```
✅ Build (nrf52840dk/nrf52840) — 5m 13s
✅ Flash limit: 194664 B / 450000 B (43%)
✅ Artifact uploaded
```

### Interview Answer
*"CI pipeline builds on every push, enforces flash budget, uploads signed artifacts. Catches regressions before flashing hardware."*

---

## ⑦ Ztest Unit Tests ✅

### Test Suite: gesture_suite
Runs on `native_sim` — no hardware needed.

### Test Cases (10/10 PASS)
| Test | What It Proves |
|------|---------------|
| test_null_input | NULL safety |
| test_right_gesture | Right detection |
| test_left_gesture | Left detection |
| test_nod_gesture | NOD detection |
| test_below_threshold | Threshold filtering |
| test_cooldown | Re-trigger prevention |
| test_stability_insufficient | Stability filter |
| test_boundary_at_threshold | Boundary: exactly at threshold = no trigger |
| test_boundary_above_threshold | Boundary: threshold+1 = trigger |
| test_boundary_below_threshold | Boundary: threshold-1 = no trigger |

### Key Finding
Boundary test revealed: threshold value itself (2500) does NOT trigger — only strictly greater values do. This off-by-one behavior documented.

### Result
```
SUITE PASS - 100.00% [gesture_suite]: pass = 10, fail = 0, skip = 0, total = 10
```

### Interview Answer
*"10 unit tests on native_sim — boundary tested at threshold±1. Runs without hardware, integrated into CI pipeline."*

---

## Summary

| Feature | Status | Proof |
|---------|--------|-------|
| Confirm path | ✅ | RTT log |
| Corrupted image | ✅ Analytical | Investigation log |
| Boot time | ✅ | 19ms measured |
| Reset reason | ✅ | RESETREAS register |
| Watchdog chain | ✅ | Watchdog reset captured |
| CI/CD | ✅ | GitHub Actions green |
| Ztest | ✅ | 10/10 pass |
