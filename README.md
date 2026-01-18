# BLE-zephyr-hearable-system
Zephyr RTOS–based BLE hearable system using nRF52840. IMU gesture detection with custom GATT, low-power design, and MCUboot OTA rollback.

Development Environment Issue (WSL I/O Error)
Problem Summary

During the development phase, a critical issue occurred in the local development environment based on WSL2 (Ubuntu), which prevented further progress of the project at this stage.

Observed Symptoms

The following symptoms were consistently observed:

All basic Linux commands failed with Input/output error

Examples:

/bin/pwd

/bin/ls

/usr/bin/df

/usr/bin/uptime

/usr/bin/which

The error occurred even when using absolute paths, indicating that it was not a PATH or shell configuration issue.

Previously functional tools (west, ls, df) suddenly became unreadable.

The system failed to execute binaries located in both /bin and /usr/bin.

Example error output:

-bash: /bin/ls: Input/output error
-bash: /usr/bin/df: Input/output error

Environment State

OS: Windows + WSL2 (Ubuntu)

Zephyr SDK and workspace were already installed and previously functional.

The issue occurred after successful Zephyr configuration and partial build attempts.

The WSL distribution was shown as Stopped, and after restart, filesystem I/O errors persisted.

Initial Diagnosis

Based on the observed behavior, this issue is not related to the application code or Zephyr configuration.
It is highly likely caused by one of the following:

Corruption or instability of the WSL virtual disk (ext4.vhdx)

Windows-side filesystem or storage issue affecting WSL

Unexpected WSL shutdown or I/O failure

Impact

Due to this issue:

Further compilation and build steps cannot be executed.

Debugging or recovery from within the WSL environment is not possible.

Project development is temporarily blocked until the WSL environment is restored or reinstalled.

Planned Resolution

The following recovery actions are planned:

Shutdown and restart of WSL (wsl --shutdown)
## Known Issue: Zephyr SDK toolchain crash in WSL (Internal Compiler Error)

During the initial build stage on WSL2 (Ubuntu), the build fails before compiling the application code.
CMake is unable to validate the toolchain because the Zephyr SDK compiler crashes while compiling a dummy C/C++ file.

Observed errors in `build/CMakeFiles/CMakeConfigureLog.yaml`:
- `arm-zephyr-eabi-g++: error: unrecognized command-line option '--target=arm-arm-none-eabi'`
- `arm-zephyr-eabi-gcc: internal compiler error: Segmentation fault ... terminated program cc1`
- `arm-zephyr-eabi-g++: internal compiler error: Segmentation fault ... terminated program cc1plus`

Impact:
- `west build` cannot proceed (toolchain validation fails).
- This is an environment/toolchain stability issue, not application source code logic.

Planned mitigation:
- Re-extract/reinstall Zephyr SDK and rerun a minimal compiler self-test.
- Clear Zephyr cache (`~/.cache/zephyr`) and rebuild.
- If persistent, migrate to a clean WSL distro or native Linux environment.


If the issue persists, full reinstallation of the WSL Ubuntu distribution

Restoration of the project repository from GitHub after environment recovery

Note: Since all project source code is version-controlled in GitHub, no source code loss is expected.
## Known Issue: Zephyr SDK toolchain crash on WSL2 (ICE / toolchain validation failure)

### Summary
On WSL2 (Ubuntu), `west build` may fail **before any application code is compiled**.  
CMake cannot validate the toolchain because the Zephyr SDK compiler crashes while compiling a **dummy C/C++ try-compile** during the initial configuration stage.

This is a **host environment / toolchain stability** issue (e.g., WSL2 storage/resource pressure, corrupted SDK extraction), not an application logic problem.

---

### Symptoms / Evidence
Build stops during compiler identification / try-compile (CMake toolchain validation).  
`build/CMakeFiles/CMakeConfigureLog.yaml` may contain errors such as:

```text
arm-zephyr-eabi-g++: error: unrecognized command-line option '--target=arm-arm-none-eabi'
arm-zephyr-eabi-gcc: internal compiler error: Segmentation fault ... terminated program cc1
arm-zephyr-eabi-g++: internal compiler error: Segmentation fault ... terminated program cc1plus
Impact
west build cannot proceed (toolchain validation fails).

Fails even before compiling project source → indicates SDK/host instability.

Quick Verification (minimal compiler self-test)
If the toolchain is healthy, compiling a trivial file should succeed:


cat > /tmp/t.c <<'EOF'
int main(void){return 0;}
EOF

$ZEPHYR_SDK_INSTALL_DIR/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc \
  -c /tmp/t.c -o /tmp/t.o
If this fails (ICE/segfault), treat it as an SDK/WSL environment problem.

Mitigation / Recovery Steps
Re-extract / reinstall Zephyr SDK
(corruption is common when disk is tight or IO is unstable)

Clear Zephyr cache + rebuild


rm -rf ~/.cache/zephyr
rm -rf build
west build -p auto -b nrf52840dk/nrf52840
Check Windows host disk space
Recommended: keep 20GB+ free to avoid WSL2 virtual disk pressure.

If the problem persists:

migrate to a clean WSL distro instance, or

use a native Linux environment

Worst case:

full reinstall of the WSL Ubuntu distribution and re-setup Zephyr

Repository Safety Note
All project source is version-controlled in GitHub, so no source code loss is expected.
After environment recovery, restore the workspace by re-cloning the repository:


git clone https://github.com/minoverse/BLE-zephyr-hearable-system.git
