# Documented API decisions

> Release context: see [qualified platforms, runtime requirements and package scope](../RELEASE-QUALIFICATION.md).


References reviewed during implementation; production executable performs no documentation/network requests.

* [GetPwrCapabilities](https://learn.microsoft.com/en-us/windows/win32/api/powrprof/nf-powrprof-getpwrcapabilities) and [SYSTEM_POWER_CAPABILITIES](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-system_power_capabilities): OS-reported capability context and AoAc.
* [SetupDiGetDeviceRegistryPropertyW](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetdeviceregistrypropertyw) and [CM_POWER_DATA](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-cm_power_data): documented device power property, not direct Registry reverse engineering.
* [DevicePowerEnumDevices](https://learn.microsoft.com/en-us/windows/win32/api/powrprof/nf-powrprof-devicepowerenumdevices): present and wake-enabled filters and documented wake predicates. [Using the Device Power API](https://learn.microsoft.com/en-us/windows/win32/power/using-the-device-power-api) explains read-only list lifecycle; no setter from its example is used.
* [EvtQuery](https://learn.microsoft.com/en-us/windows/win32/api/winevt/nf-winevt-evtquery), [EvtNext](https://learn.microsoft.com/en-us/windows/win32/api/winevt/nf-winevt-evtnext), [EvtRender](https://learn.microsoft.com/en-us/windows/win32/api/winevt/nf-winevt-evtrender): local bounded reverse query and XML rendering. Provider payloads can vary; unknown numeric types are not reverse engineered.
* [ITaskService::Connect](https://learn.microsoft.com/en-us/windows/win32/api/taskschd/nf-taskschd-itaskservice-connect), [ITaskSettings::get_WakeToRun](https://learn.microsoft.com/en-us/windows/win32/api/taskschd/nf-taskschd-itasksettings-get_waketorun), [IRegisteredTask::get_NextRunTime](https://learn.microsoft.com/en-us/windows/win32/api/taskschd/nf-taskschd-iregisteredtask-get_nextruntime): local-only task metadata, no actions or task modifications.
* [POWER_INFORMATION_LEVEL](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntpoapi/ne-ntpoapi-power_information_level): the WakeTimerList enum name is not a complete supported user-mode contract. No private timer/source structures or undocumented calls are used.
* [XmlLite security](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ms752611(v=vs.85)): DTD prohibited, no external resolver installed, bounded input and depth.
