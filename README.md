# wake-source-auditor 1.0.0

> Release context: see [qualified platforms, runtime requirements and package scope](RELEASE-QUALIFICATION.md).


A native Windows command-line auditor for locally available wake evidence. It explains what Windows reports and what remains unknown. **Absence of evidence is not proof that no wake occurred.**

```text
wake-source-auditor
wake-source-auditor --last-wake
wake-source-auditor --devices
wake-source-auditor --timers
wake-source-auditor --json
wake-source-auditor --devices --timers --json
```

`--help` and `--version` run alone. Focus switches may be combined; default queries all sources. JSON stdout is clean UTF-8 without a BOM. Human console output uses Unicode Windows console writes; redirected output is UTF-8. Human device/task lists are capped at 40/20 observations; JSON contains all collected observations within acquisition budgets.

## Evidence and limits

* **Reported wake event:** local System log, Microsoft-Windows-Power-Troubleshooter event 1. Query is reverse ordered, limited to the last 30 days and at most 32 matching events. The newest usable event is shown, including UTC event timestamp, provider, ID, record ID and available SleepTime, WakeTime, WakeSourceType, WakeSourceText, WakeTimerOwner and WakeTimerContext fields. Missing text means unknown cause. Numeric source types are retained without undocumented interpretation. An event reports a source; the auditor does not independently establish causality. Localized text is preserved.
* **Configured capability:** present device `CM_POWER_DATA.PD_Capabilities` wake-from-D0/D1/D2/D3 flags through SetupAPI. These are device power-state capabilities, not proof the platform can wake from every system sleep state. Instance ID, name and class are provided where available. Raw power flags are available in JSON. Missing properties mean incomplete coverage, not incapability.
* **Configured wake behavior:** separate Device Power API enumeration of present devices with `DEVICEPOWER_FILTER_WAKEENABLED`, using documented D0-D3/S0-S3 wake predicates. Friendly names may repeat; this list is never joined to instance IDs by name. A capability row has unknown enabled state; an enabled row reports enabled behavior independently. No absence is interpreted as disabled. The BOOLEAN enumeration API has an ambiguous end/failure contract; the limitation is always disclosed. Device list is bounded at 4096 per enumeration.
* **WakeToRun tasks:** read-only local Task Scheduler COM enumeration, including hidden and disabled tasks accessible to the caller. Each matching task includes path, enabled state and optional next scheduled run in local wall-clock time. Disabled tasks are explicitly labeled. WakeToRun is a request to wake for a task, not an active kernel wake timer, nor proof a task caused a past wake. Conditions, policy and hardware still apply. Enumeration is bounded to 4096 tasks, 256 folders, depth 16 and a 15-second budget checked between calls; a Windows API call itself can take longer. Inaccessible folders/tasks generate partial status and warnings.
* **Active kernel wake timers:** unavailable. This release does not use undocumented buffer layouts or private interfaces behind `powercfg`. The presence of a WakeTimerList enum identifier alone is insufficient as a documented user-mode enumeration contract. `--timers` explains this and reports configured tasks. Null timer items mean unknown, not zero active timers.
* **Power context:** `GetPwrCapabilities` exposes S1/S2/S3, S4 hibernation, hibernation file presence, AoAc (Modern Standby) and wake alarm capability. These are OS-reported capabilities, not a sleep test or a full analysis of policy/firmware restrictions.

Event logs may be disabled, unavailable, cleared, inaccessible, or outside the lookback period. The tool never enables channels or changes retention. Modern Standby transitions and firmware/platform activity may not appear as traditional resume events. This is not a full Modern Standby diagnostic. No event-name/device/task correlation is asserted as proof. A server or virtual machine may expose no sleep capability at all; do not infer physical hardware behavior from a VPS. Local power capabilities provide context, not proof of a wake.

## Safety and compatibility

Production behavior is strictly read-only: no power/device/task/Registry/firmware configuration changes, no suspend/hibernate/reboot, no remediation and no auto-elevation (`asInvoker` manifest). No intentional network communication, telemetry, update checks, DNS, adapter probes or Wake-on-LAN transmission. Task Scheduler is connected only to the local computer using current credentials. The tool never launches `powercfg`, PowerShell, WMI tools, .NET or any subprocess. Windows system services may have their own behavior outside the application's control.

Targets Windows 10, Windows 11 and corresponding Windows Server 2016/2019/2022/2025 generations with the documented APIs available. OS vendor support lifecycle still applies. Primary tested environment is Server 2025 x64; see [validation report](validation/REPORT.md) for actual coverage. Client hardware wake behavior is not assumed validated. Administrator privileges are not required; optional sources can be incomplete under the caller's permissions. No continuous monitoring or ETW capture.

Exit codes: `0` completed (including no evidence or optional-source failure), `2` usage error, `3` required power-capability acquisition failed in the default audit, `4` unexpected internal or output failure. Focused modes do not require unrelated sources. Acquisition status is `ok`, `partial`, `unavailable` or `not_requested`; `ok` does not mean exhaustive hardware coverage. Warnings contain operation and numeric Windows/HRESULT codes without leaking task actions or credentials. JSON diagnostics are in the document; an internal error writes stderr and returns 4 without emitting a misleading successful document.

## Build and test

Requires CMake 3.24+, MSVC supporting C++17 and Windows SDK. No third-party libraries or network downloads. Run from an x64 MSVC developer prompt. Ninja is used for the validated builds; Visual Studio generators can also be used with the corresponding installed toolchain.

```text
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
cmake --install build-release --prefix install
```

`-DWSA_ANALYZE=ON` enables `/analyze`; `/W4 /WX` is always enabled. Use a separate build with `-DWSA_ASAN=ON` for MSVC ASan (Release or RelWithDebInfo; avoid incompatible `/RTC` Debug settings). The matching compiler ASan runtime DLL must be available beside test executables or through the loader's supported search path. Synthetic CTest validates parsing, evidence distinctions, Unicode and JSON escaping. Integration CTest makes only real read-only queries; it deliberately tolerates absent event/device/task evidence. `wsa_tests --restricted-integration` verifies that the current process is non-admin at medium integrity, then runs the same acquisition. It is intended to be run from a standard-user session. An independent JSON parser and non-admin execution are part of validation, outside the app. See [JSON contract](docs/JSON.md), [API references](docs/API-SOURCES.md), and [validation report](validation/REPORT.md).

MIT licensed.
