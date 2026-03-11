# Kernel Panic Investigation: sem.c:136 Deep Dive

## Symptom
```
ASSERTION FAIL @ kernel/sem.c:136
Fault during interrupt handling
PC: 0x00019a9a  LR: 0x00016153
XPSR: 0x01000021 (IRQ17 = RTC1)
```

## Root Cause Chain (Final Answer)
```
RTC1 interrupt fires (tickless kernel wakeup)
→ _isr_wrapper() detects idle state
→ pm_system_resume() called FROM ISR context
→ pm_resume_devices() iterates all PM devices
→ qspi_nor_pm_action(RESUME) → qspi_lock() → k_sem_take(K_FOREVER)
→ ASSERTION: arch_is_in_isr() == true, timeout != K_NO_WAIT
```

## Investigation Timeline

### Attempt 1: RTT Console Double Init
**Hypothesis:** CONFIG_RTT_CONSOLE=y + CONFIG_LOG_BACKEND_RTT=y  
**Fix:** CONFIG_RTT_CONSOLE=n  
**Result:** Different crash, not root cause

### Attempt 2: LOG_MODE_IMMEDIATE in Timer Callback
**Hypothesis:** LOG_INF() in k_timer callback → k_mutex_lock(K_FOREVER) in ISR  
**Fix:** Removed CONFIG_LOG_MODE_IMMEDIATE=y  
**Result:** Partially fixed, crash moved to different address

### Attempt 3: bt_conn_le_param_update() in Timer ISR
**Hypothesis:** ble_adaptive.c update_policy() calling BT API directly  
**Fix:** Deferred via k_work_submit()  
**Result:** Fixed that crash, new crash appeared

### Attempt 4: QSPI NOR Flash PM Resume (ROOT CAUSE)
**Hypothesis:** nRF52840dk BSP enables MX25R6435F QSPI flash by default.  
CONFIG_PM_DEVICE=y causes pm_resume_devices() from ISR context.  
QSPI driver resume calls k_sem_take(K_FOREVER) → assertion.

**Fix 1:** Disabled QSPI in overlay
```
&qspi { status = "disabled"; };
```
**Result:** Crash shifted to LSM6DSO sensor (same pattern)

**Fix 2 (Final):** CONFIG_PM_DEVICE=n
```
# CONFIG_PM_DEVICE intentionally disabled
# pm_resume_devices() called from ISR context via _isr_wrapper
# Any device resume with I2C/SPI calls k_sem_take() → assertion
CONFIG_PM_DEVICE=n
```
**Result:** ✅ Crash eliminated permanently

## Why CONFIG_PM vs CONFIG_PM_DEVICE

| Config | Effect | Safe from ISR? |
|--------|--------|---------------|
| CONFIG_PM=y | CPU suspend-to-idle (WFI) | ✅ Yes |
| CONFIG_PM_DEVICE=y | Device suspend/resume hooks | ❌ No — resumes from ISR |

CPU sleep works through CONFIG_PM alone. Device PM not needed.

## All Bugs Found and Fixed

| # | File | Bug | Fix |
|---|------|-----|-----|
| 1 | prj.conf | CONFIG_PM_DEVICE=y → ISR semaphore crash | CONFIG_PM_DEVICE=n |
| 2 | ble_adaptive.c | bt_conn_le_param_update() in timer ISR | k_work_submit() defer |
| 3 | adaptive_power.c | LOG_INF() in timer callback | Removed log from ISR |
| 4 | power_stats.c | LOG_DBG() in PM notifier callback | Removed log from callback |
| 5 | gesture_service.c | pending_conn ref leak on reconnect | bt_conn_unref() guard |
| 6 | CMakeLists.txt | dwt_profiler.c missing | Removed from build |
| 7 | prj.conf | CONFIG_RTT_CONSOLE=y double init | CONFIG_RTT_CONSOLE=n |
| 8 | prj.conf | LOG_MODE_IMMEDIATE rejected by BLE stack | CONFIG_LOG_MODE_DEFERRED=y |

## Key Engineering Lesson

> nRF52840dk BSP enables QSPI NOR flash by default in device tree.
> With CONFIG_PM_DEVICE=y, ALL enabled devices are suspended/resumed automatically.
> pm_resume_devices() runs from ISR context (_isr_wrapper → pm_system_resume).
> Any driver whose resume callback performs I2C/SPI will call k_sem_take(K_FOREVER) → assertion.
> Solution: CONFIG_PM_DEVICE=n — CPU sleep is independent of device PM.

## Summary

*"The kernel panic was caused by pm_resume_devices() being called from ISR context via Zephyr's _isr_wrapper. The nRF52840dk BSP enables QSPI flash by default, and its PM resume callback calls k_sem_take(K_FOREVER) — illegal from interrupt context. The fix was CONFIG_PM_DEVICE=n: CPU suspend-to-idle works through CONFIG_PM alone without device PM hooks."*
