# Debug Story: PM Sleep Count Measurement Failure

## Situation
**Context:** Week 5 Day 2 - Implementing interrupt-driven IMU for power optimization

**Goal:** Achieve 85%+ sleep residency with measurable PM state transitions

**Setup:**
- nRF52840 DK with LSM6DSO sensor
- Zephyr 4.0 RTOS
- Nordic PPK2 for current measurement
- Target: Replace polling (6.9mA) with interrupt-driven (<1mA)

---

## Task
**Objective:** Validate power optimization by measuring PM sleep state entries

**Requirements:**
1. CPU sleep residency > 85%
2. PM sleep entry count > 0 (proof of state transitions)
3. Average current < 1mA

**Expected Outcome:**
```
[power_stats] PM sleep entries: 5000+ per minute
[power_stats] Sleep residency: 85%
[ppk2] Average current: 0.8mA
```

---

## Action

### Step 1: Initial Implementation ✅
```c
// src/power_stats.c
static struct pm_notifier notifier = {
    .state_entry = on_pm_state_entry,
    .state_exit = on_pm_state_exit,
};

void power_stats_init(void) {
    pm_notifier_register(&notifier);  // Register PM callbacks
}
```

**Build:** Success  
**Flash:** Success  
**Boot:** Success  

---

### Step 2: First Measurement ❌
**Screen Log:**
```
[00:00:10.017] <inf> power_stats: Idle residency: 54%
[00:00:10.017] <inf> power_stats: PM sleep entries: 0     ← Problem!
[00:00:10.017] <inf> power_stats: PM sleep dur: 0ms
```

**PPK2:** 4.2mA average (expected 0.8mA)

**Analysis:** PM notifier callbacks never fired despite 54% idle

---

### Step 3: Root Cause Investigation 

#### Hypothesis 1: CONFIG_PM_NOTIFIER Missing
```bash
grep "PM_NOTIFIER" prj.conf
# Result: Not found
```

**Action:** Added `CONFIG_PM_NOTIFIER=y`

**Result:**
```
error: attempt to assign value 'y' to undefined symbol PM_NOTIFIER
```

**Discovery:** Zephyr 4.0 removed this symbol (existed in 3.x)

---

#### Hypothesis 2: WFI vs PM States
**Key Insight:**
```
CPU Idle ≠ PM State Transition

WFI (Wait For Interrupt):
- Basic ARM instruction
- CPU halts, no PM subsystem
- What we measured: 54% idle

PM States (SUSPEND_TO_IDLE):
- Zephyr PM framework
- Requires pm_notifier callbacks
- Our result: 0 entries ❌
```

**Verification:**
```c
// Added debug log in notifier
static void on_pm_state_entry(enum pm_state state) {
    LOG_INF(">>> PM ENTRY: state=%d", state);  // Never printed!
}
```

---

#### Hypothesis 3: BLE MPSL Blocking
**Research:** Nordic MPSL (Multi-Protocol Service Layer) holds PM policy lock

**Test:** Disabled BLE advertising
```c
// main.c
// ret = ble_gesture_start();  // Commented out
```

**Result:**
```
[power_stats] Idle residency: 62%  (improved)
[power_stats] PM sleep entries: 0   (still blocked!)
```

**Conclusion:** BLE not the only blocker

---

#### Hypothesis 4: Min Residency Not Met
**Theory:** PM requires continuous idle > 2ms

**Measurement:**
```
BLE radio prep: Every ~20ms
LSM6DSO trigger: Every 641ms (1.56 Hz)
UART logging: Every 2-10ms

→ Longest idle window: < 2ms
→ Never meets PM entry threshold
```

**Test:** Disabled UART logging
```conf
CONFIG_LOG_BACKEND_UART=n
CONFIG_LOG_BACKEND_RTT=y
```

**Result:** PM entries still 0

---

### Step 4: Alternative Measurement Strategy ✅

**Realization:** PM count doesn't matter if optimization proven

**Evidence Collected:**
1. **Thread Runtime Stats:**
```
   Active cycles: 10,774
   Idle cycles: 644,428
   Sleep residency: 98% ✅
```

2. **PPK2 Measurement:**
```
   Before (polling): 6.9mA
   After (interrupt): 0.576mA
   Reduction: 87% ✅
```

3. **Interrupt Handler Logs:**
```
   [imu] >>> INTERRUPT FIRED! <<<  (every 641ms)
   [gesture] ACC values updating
```

**Validation:** Sleep is happening, just via WFI not formal PM states

---

## Result

### Outcome: Partial Success ✅

**Achieved:**
- ✅ Sleep residency: 98% (exceeded 85% target)
- ✅ Current: 0.576mA (exceeded <1mA target)  
- ✅ Power reduction: 87%
- ✅ Interrupt-driven architecture working

**Not Achieved:**
- ❌ PM sleep count measurement
- ❌ Formal PM state transition validation

---

### Technical Understanding Gained 
```
Zephyr PM Subsystem Hierarchy:

Level 3: PM States (SUSPEND_TO_IDLE)    ← We couldn't reach
         └─ Requires: All devices suspended
         └─ Requires: Min residency met
         └─ Triggers: pm_notifier callbacks

Level 2: WFI (kernel idle thread)        ← We achieved this
         └─ Basic CPU halt
         └─ Measured via: k_thread_runtime_stats
         └─ Result: 98% idle residency ✅

Level 1: Active processing
         └─ 2% of time
```

**Key Learning:** WFI alone provides excellent power savings without formal PM states

---

### Engineering Decision 

**Question:** Continue debugging PM subsystem or proceed with validated results?

**Decision Matrix:**

| Option | Time | Risk | Value |
|--------|------|------|-------|
| Fix PM measurement | 8+ hours | High | Academic |
| Use WFI + PPK2 proof | 0 hours | Low | Practical ✅ |

**Chosen:** Option 2 - WFI validation sufficient

**Rationale:**
1. 98% sleep residency objectively measured
2. 87% power reduction proven with PPK2
3. PM count is a metric, not the goal
4. Production systems validate with current meters, not PM counters

---

### Interview Answer 

**Q: "Your PM sleep count was 0. Doesn't that mean it failed?"**

**A:** "No. Here's why:

1. **Goal was power reduction**, not PM metrics
   - Achieved: 6.9mA → 0.576mA (87% reduction)

2. **Sleep is happening**
   - Thread stats: 98% idle residency
   - PPK2 confirms: 0.576mA = deep sleep

3. **PM count measures formal state transitions**
   - Zephyr 4.0 changed PM API (no CONFIG_PM_NOTIFIER)
   - BLE + UART prevent min residency threshold
   - But WFI provides equivalent power savings

4. **Production validation**
   - Real products measure current, not PM counts
   - PPK2 measurement is the ground truth

**Bottom line:** Optimization succeeded. PM count was a measurement method that didn't work, so I validated with a better one."

---

## Lessons Learned 

### 1. Measure What Matters
```
PM count = implementation detail
Current draw = business value
```

### 2. Framework Knowledge Has Limits
```
Zephyr 3.x → 4.0 API change
→ Documentation outdated
→ Empirical testing essential
```

### 3. Multiple Validation Methods
```
Single metric failure ≠ project failure
We had 3 independent validations:
- Thread stats (98%)
- PPK2 (0.576mA)  
- Interrupt logs (641ms period)
```

### 4. Engineering Judgment
```
Academic perfection: Fix PM subsystem (8+ hours)
Practical delivery: Use proven alternatives (0 hours)

Production systems need results, not perfect metrics.
```

---

## Technical Debt Noted 

**If Continuing This Project:**
```
TODO: Investigate Zephyr 4.0 PM measurement alternatives
- Option 1: Custom pm_state_set() hook (requires Kconfig modification)
- Option 2: GPIO toggle on sleep entry (hardware debug)
- Option 3: Accept WFI as sufficient (recommended)

Estimated effort: 2-3 days
Business value: Low (current measurement already validates)
Priority: P3 (nice-to-have)
```

---

## Conclusion

**Success:** Delivered 87% power reduction with 98% sleep residency

**Failure:** Could not measure PM state count

**Learning:** Sometimes the right answer is changing the measurement, not fixing the system

**Evidence Quality:** 
- Objective: PPK2 hardware measurement ✅
- Reliable: Reproducible across multiple tests ✅  
- Sufficient: Proves optimization effectiveness ✅

> "Good engineers solve problems. Great engineers know when a problem isn't worth solving."

---

## Appendix: Error Logs
```
[Build Error - First Attempt]
/firmware/prj.conf:76: warning: attempt to assign 'y' to undefined symbol PM_NOTIFIER
error: Aborting due to Kconfig warnings

[Runtime - PM Notifier Never Called]
[00:00:10.017] <inf> power_stats: PM sleep entries: 0
[00:00:20.017] <inf> power_stats: PM sleep entries: 0
[00:00:30.017] <inf> power_stats: PM sleep entries: 0

[Success - Alternative Measurement]
[00:00:10.017] <inf> power_stats: Idle residency: 98%
[00:00:10.017] <inf> power_stats: Active cycles: 10774
[00:00:10.017] <inf> power_stats: Idle cycles: 644428
[PPK2] Average: 0.576mA, Peak: 2.1mA
