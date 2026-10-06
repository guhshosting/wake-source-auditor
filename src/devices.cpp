#include "wsa/model.hpp"
#include <windows.h>
#include <setupapi.h>
#include <powrprof.h>
#include <algorithm>
namespace wsa {
namespace {
class DeviceSet {
    HDEVINFO value_;
public:
    DeviceSet() : value_(SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT)) {}
    ~DeviceSet() { if (value_ != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(value_); }
    HDEVINFO get() const { return value_; }
};
std::string property(HDEVINFO set, SP_DEVINFO_DATA& data, DWORD key) {
    DWORD bytes = 0, type = 0;
    SetupDiGetDeviceRegistryPropertyW(set, &data, key, &type, nullptr, 0, &bytes);
    if (bytes == 0 || bytes > 65536) return {};
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    if (!SetupDiGetDeviceRegistryPropertyW(set, &data, key, &type, reinterpret_cast<BYTE*>(buffer.data()), bytes, nullptr) || type != REG_SZ) return {};
    return utf8(std::wstring(buffer.data()));
}
class PowerList {
public:
    bool opened = DevicePowerOpen(0) != FALSE;
    ~PowerList() { if (opened) DevicePowerClose(); }
};
}
void acquire_devices(Audit& audit) {
    audit.devices_source.status = "ok";
    DeviceSet set;
    if (set.get() == INVALID_HANDLE_VALUE) {
        audit.devices_source.status = "partial";
        audit.warnings.push_back(error_code("SetupDiGetClassDevsW", GetLastError()));
    } else {
        for (DWORD index = 0; index < 4096; ++index) {
            SP_DEVINFO_DATA data{}; data.cbSize = sizeof(data);
            if (!SetupDiEnumDeviceInfo(set.get(), index, &data)) {
                const auto code = GetLastError();
                if (code != ERROR_NO_MORE_ITEMS) { audit.devices_source.status = "partial"; audit.warnings.push_back(error_code("SetupDiEnumDeviceInfo", code)); }
                break;
            }
            CM_POWER_DATA power{}; DWORD type = 0, needed = 0;
            if (!SetupDiGetDeviceRegistryPropertyW(set.get(), &data, SPDRP_DEVICE_POWER_DATA, &type,
                reinterpret_cast<BYTE*>(&power), sizeof(power), &needed)) {
                const auto code = GetLastError();
                // A missing optional property is common and does not imply no capability.
                if (code != ERROR_INVALID_DATA && code != ERROR_NOT_SUPPORTED) {
                    audit.devices_source.status = "partial";
                    if (audit.warnings.size() < 64) audit.warnings.push_back(error_code("Device power property", code));
                }
                continue;
            }
            if (type != REG_BINARY || needed < sizeof(power) || power.PD_Size < sizeof(power)) {
                audit.devices_source.status = "partial"; audit.warnings.emplace_back("Unexpected device power property; skipped."); continue;
            }
            constexpr ULONG wake_mask = PDCAP_WAKE_FROM_D0_SUPPORTED | PDCAP_WAKE_FROM_D1_SUPPORTED | PDCAP_WAKE_FROM_D2_SUPPORTED | PDCAP_WAKE_FROM_D3_SUPPORTED;
            if ((power.PD_Capabilities & wake_mask) == 0) continue;
            std::vector<wchar_t> id(4096, L'\0');
            if (!SetupDiGetDeviceInstanceIdW(set.get(), &data, id.data(), static_cast<DWORD>(id.size()), nullptr)) {
                audit.devices_source.status = "partial"; audit.warnings.push_back(error_code("SetupDiGetDeviceInstanceIdW", GetLastError())); continue;
            }
            Device device;
            device.identity = utf8(std::wstring(id.data())); device.identity_kind = "device_instance_id";
            device.name = property(set.get(), data, SPDRP_FRIENDLYNAME);
            if (device.name.empty()) device.name = property(set.get(), data, SPDRP_DEVICEDESC);
            device.device_class = property(set.get(), data, SPDRP_CLASS);
            device.category = "configured_capability"; device.wake_capable = true;
            device.power_flags = power.PD_Capabilities;
            device.source = "SetupDiGetDeviceRegistryPropertyW: CM_POWER_DATA.PD_Capabilities wake-from-D0/D1/D2/D3 flags";
            audit.devices.push_back(std::move(device));
            if (index == 4095) { audit.devices_source.status = "partial"; audit.warnings.emplace_back("Device enumeration capped at 4096 present devices."); }
        }
    }
    PowerList list;
    if (!list.opened) {
        audit.devices_source.status = "partial";
        audit.warnings.push_back(error_code("DevicePowerOpen (enabled-device list unavailable)", GetLastError())); return;
    }
    // These flags cover documented D0-D3 and S0-S3 wake capability predicates.
    constexpr ULONG wake_mask = PDCAP_WAKE_FROM_D0_SUPPORTED | PDCAP_WAKE_FROM_D1_SUPPORTED |
        PDCAP_WAKE_FROM_D2_SUPPORTED | PDCAP_WAKE_FROM_D3_SUPPORTED |
        PDCAP_WAKE_FROM_S0_SUPPORTED | PDCAP_WAKE_FROM_S1_SUPPORTED | PDCAP_WAKE_FROM_S2_SUPPORTED | PDCAP_WAKE_FROM_S3_SUPPORTED;
    for (ULONG index = 0; index < 4096; ++index) {
        std::vector<wchar_t> name(32768, L'\0');
        ULONG bytes = static_cast<ULONG>(name.size() * sizeof(wchar_t));
        SetLastError(ERROR_SUCCESS);
        if (!DevicePowerEnumDevices(index, DEVICEPOWER_FILTER_DEVICES_PRESENT | DEVICEPOWER_FILTER_WAKEENABLED,
            wake_mask, reinterpret_cast<BYTE*>(name.data()), &bytes)) {
            const auto code = GetLastError();
            // The documented BOOLEAN interface does not specify a distinct end-of-list error.
            if (code != ERROR_SUCCESS && code != ERROR_NO_MORE_ITEMS) {
                audit.devices_source.status = "partial"; audit.warnings.push_back(error_code("DevicePowerEnumDevices", code));
            }
            break;
        }
        const auto end = std::find(name.begin(), name.end(), L'\0');
        if (end == name.end() || end == name.begin()) {
            audit.devices_source.status = "partial"; audit.warnings.emplace_back("Invalid enabled-device name; enumeration stopped."); break;
        }
        Device device;
        device.name = utf8(std::wstring(name.begin(), end)); device.identity = device.name;
        device.identity_kind = "non_unique_friendly_name";
        device.category = "configured_wake_behavior"; device.wake_enabled = true;
        device.source = "DevicePowerEnumDevices: present + WAKEENABLED, any documented D0-D3/S0-S3 wake flag";
        audit.devices.push_back(std::move(device));
        if (index == 4095) { audit.devices_source.status = "partial"; audit.warnings.emplace_back("Enabled-device list capped at 4096."); }
    }
    audit.limitations.emplace_back("Device power properties can be absent. Enabled-device enumeration is scoped to documented D0-D3/S0-S3 wake flags; its BOOLEAN API does not unambiguously distinguish an empty list from every possible failure. No negative enabled-state inference is made.");
}
}
