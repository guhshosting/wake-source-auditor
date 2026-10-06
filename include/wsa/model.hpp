#ifndef WSA_MODEL_HPP
#define WSA_MODEL_HPP
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace wsa {
inline constexpr const char* version = "1.0.0";
struct Source {
    std::string status = "not_requested"; // ok, partial, unavailable, not_requested
    std::string api;
};
struct Power {
    Source source{"not_requested", "GetPwrCapabilities"};
    bool s1 = false, s2 = false, s3 = false, s4 = false;
    bool hiber_file = false, modern_standby = false, wake_alarm = false;
};
// MSVC std::map move can allocate a sentinel; this implicit move is allowed to throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct WakeEvent {
    std::string provider, timestamp, event_id, record_id;
    std::map<std::string, std::string> fields;
    std::string explanation;
};
struct Device {
    std::string identity, identity_kind, name, device_class;
    std::string category; // configured_capability or configured_wake_behavior
    std::optional<bool> wake_capable, wake_enabled;
    std::optional<unsigned long> power_flags;
    std::string source;
};
struct Task {
    std::string path;
    bool enabled = false;
    std::optional<std::string> next_run_local;
};
// Contains optional<WakeEvent>; its implicit move is allowed to throw, too.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct Audit {
    Power power;
    Source events{"not_requested", "EvtQuery/EvtNext/EvtRender: local System log"};
    Source devices_source{"not_requested", "SetupDiGetDeviceRegistryPropertyW(SPDRP_DEVICE_POWER_DATA); DevicePowerEnumDevices"};
    Source tasks_source{"not_requested", "local Task Scheduler COM: ITaskSettings::get_WakeToRun"};
    std::optional<WakeEvent> last_wake;
    std::vector<Device> devices;
    std::vector<Task> tasks;
    std::vector<std::string> warnings, limitations;
    bool required_failure = false;
};
struct Options { bool power = true, events = true, devices = true, tasks = true, json = false; };
std::string utf8(const std::wstring& value);
std::string error_code(const std::string& operation, unsigned long code);
std::string explain_wake(const WakeEvent& event);
std::string explain_power(const Power& power);
Audit collect(const Options& options);
void acquire_power(Audit& audit);
void acquire_events(Audit& audit);
void acquire_devices(Audit& audit);
void acquire_tasks(Audit& audit);
std::optional<WakeEvent> parse_wake_xml(const std::wstring& xml, std::string& error);
std::string json(const Audit& audit);
std::string human(const Audit& audit);
std::string quote_json(const std::string& text);
}
#endif
