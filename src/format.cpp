#include "wsa/model.hpp"
#include <algorithm>
#include <sstream>
namespace wsa {
std::string quote_json(const std::string& text) {
    std::string out = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char ch : text) {
        if (ch == '"' || ch == '\\') { out += '\\'; out += static_cast<char>(ch); }
        else if (ch < 0x20) { out += "\\u00"; out += hex[static_cast<unsigned>(ch) >> 4U]; out += hex[static_cast<unsigned>(ch) & 15U]; }
        else out += static_cast<char>(ch);
    }
    return out + '"';
}
namespace {
const char* boolean(bool value) { return value ? "true" : "false"; }
std::string nullable_bool(std::optional<bool> value) { return value ? boolean(*value) : "null"; }
std::string source(const Source& s) { return "{\"status\":" + quote_json(s.status) + ",\"source\":" + quote_json(s.api) + "}"; }
void strings(std::ostringstream& out, const std::vector<std::string>& values) {
    out << '[';
    for (size_t i = 0; i < values.size(); ++i) { if (i) out << ','; out << quote_json(values[i]); }
    out << ']';
}
// Local names and event fields must not inject terminal control sequences.
std::string safe(const std::string& value) {
    std::string out;
    size_t bytes = (std::min)(value.size(), size_t{512});
    while (bytes < value.size() && bytes > 0 && (static_cast<unsigned char>(value[bytes]) & 0xc0U) == 0x80U) --bytes;
    for (unsigned char ch : value.substr(0, bytes)) {
        if (ch < 0x20 || ch == 0x7f) out += '?'; else out += static_cast<char>(ch);
    }
    if (bytes < value.size()) out += "... [truncated; full value in --json]";
    return out;
}
}
std::string json(const Audit& a) {
    std::ostringstream out;
    out << "{\"schema_version\":\"1.0\",\"application_version\":" << quote_json(version)
        << ",\"audit_status\":" << quote_json(a.required_failure ? "required_source_failure" : "completed")
        << ",\"power_capabilities\":{\"acquisition\":" << source(a.power.source);
    auto power_flag = [&](const char* key, bool value) { out << ",\"" << key << "\":" << (a.power.source.status == "ok" ? boolean(value) : "null"); };
    power_flag("s1", a.power.s1); power_flag("s2", a.power.s2); power_flag("s3", a.power.s3); power_flag("s4_hibernation", a.power.s4);
    power_flag("hibernation_file_present", a.power.hiber_file); power_flag("modern_standby_aoac", a.power.modern_standby); power_flag("wake_alarm_present", a.power.wake_alarm);
    out << ",\"evidence_category\":\"configured_capability\",\"explanation\":" << quote_json(explain_power(a.power)) << "},\"last_wake\":{\"acquisition\":" << source(a.events)
        << ",\"lookback_days\":30,\"max_events\":32,\"event\":";
    if (!a.last_wake) out << "null";
    else {
        const auto& e = *a.last_wake;
        out << "{\"evidence_category\":\"reported_wake_event\",\"source\":\"local System Event Log\",\"provider\":" << quote_json(e.provider)
            << ",\"event_id\":" << quote_json(e.event_id) << ",\"record_id\":" << quote_json(e.record_id)
            << ",\"timestamp_utc\":" << quote_json(e.timestamp) << ",\"fields\":{";
        bool comma = false;
        for (const auto& [key, value] : e.fields) { if (comma) out << ','; comma = true; out << quote_json(key) << ':' << quote_json(value); }
        out << "},\"explanation\":" << quote_json(e.explanation) << '}';
    }
    out << "},\"devices\":{\"acquisition\":" << source(a.devices_source) << ",\"items\":[";
    for (size_t i = 0; i < a.devices.size(); ++i) {
        if (i) out << ',';
        const auto& d = a.devices[i];
        out << "{\"identity\":" << quote_json(d.identity) << ",\"identity_kind\":" << quote_json(d.identity_kind)
            << ",\"name\":" << quote_json(d.name) << ",\"class\":" << quote_json(d.device_class)
            << ",\"evidence_category\":" << quote_json(d.category) << ",\"wake_capable\":" << nullable_bool(d.wake_capable)
            << ",\"wake_enabled\":" << nullable_bool(d.wake_enabled) << ",\"power_capability_flags\":";
        if (d.power_flags) out << *d.power_flags; else out << "null";
        out << ",\"source\":" << quote_json(d.source) << '}';
    }
    out << "]},\"timers\":{\"status\":\"unavailable\",\"evidence_category\":\"unavailable_unknown\",\"items\":null,\"reason\":\"No documented active kernel wake-timer enumeration contract used\"},\"wake_enabled_tasks\":{\"acquisition\":" << source(a.tasks_source) << ",\"items\":[";
    for (size_t i = 0; i < a.tasks.size(); ++i) {
        if (i) out << ',';
        const auto& t = a.tasks[i];
        out << "{\"path\":" << quote_json(t.path) << ",\"wake_to_run\":true,\"enabled\":" << boolean(t.enabled)
            << ",\"next_run_local\":" << (t.next_run_local ? quote_json(*t.next_run_local) : "null")
            << ",\"evidence_category\":\"configured_wake_behavior\",\"source\":\"local Task Scheduler COM\",\"proves_wake\":false}";
    }
    out << "]},\"warnings\":"; strings(out, a.warnings);
    out << ",\"limitations\":"; strings(out, a.limitations);
    out << "}\n";
    return out.str();
}
std::string human(const Audit& a) {
    std::ostringstream out;
    out << "wake-source-auditor " << version << "\n\n";
    if (a.events.status != "not_requested") {
        out << "Most recent wake evidence [" << a.events.status << "]\n";
        if (a.last_wake) {
            const auto& e = *a.last_wake;
            out << "  " << safe(e.timestamp) << " | " << safe(e.provider) << " | event " << safe(e.event_id) << " | record " << safe(e.record_id) << '\n'
                << "  " << safe(e.explanation) << '\n';
            for (const auto& [key, value] : e.fields) if (!value.empty()) out << "  " << key << ": " << safe(value) << '\n';
        } else out << "  No usable wake evidence available in the bounded local query.\n";
    }
    if (a.devices_source.status != "not_requested") {
        out << "\nWake devices [" << a.devices_source.status << "]\n";
        if (a.devices.empty()) out << "  No matching wake device data returned; coverage may be incomplete.\n";
        const size_t limit = (std::min)(a.devices.size(), size_t{40});
        for (size_t i = 0; i < limit; ++i) {
            const auto& d = a.devices[i];
            out << "  " << (d.wake_enabled == true ? "Enabled to wake" : "Wake capability") << ": " << safe(d.name.empty() ? d.identity : d.name);
            if (!d.device_class.empty()) out << " [" << safe(d.device_class) << ']';
            out << '\n';
        }
        if (a.devices.size() > limit) out << "  " << a.devices.size() - limit << " additional observations; use --json for full collected list.\n";
    }
    if (a.tasks_source.status != "not_requested") {
        out << "\nWake-related timers/tasks [" << a.tasks_source.status << "]\n  Active kernel wake timers: unavailable through the documented contract used.\n";
        if (a.tasks.empty()) out << "  No accessible WakeToRun tasks returned.\n";
        const size_t limit = (std::min)(a.tasks.size(), size_t{20});
        for (size_t i = 0; i < limit; ++i) {
            const auto& t = a.tasks[i];
            out << "  WakeToRun: " << safe(t.path) << " (" << (t.enabled ? "enabled" : "disabled") << ')';
            if (t.next_run_local) out << "; next run " << *t.next_run_local << " local time";
            out << '\n';
        }
        if (a.tasks.size() > limit) out << "  " << a.tasks.size() - limit << " additional tasks; use --json.\n";
        out << "  Task configuration is not an active timer or proof that a task caused a wake.\n";
    }
    if (a.power.source.status != "not_requested") {
        out << "\nPower/sleep context [" << a.power.source.status << "]\n  " << explain_power(a.power) << '\n';
        if (a.power.source.status == "ok") out << "  S1=" << boolean(a.power.s1) << " S2=" << boolean(a.power.s2) << " S3=" << boolean(a.power.s3)
            << " S4=" << boolean(a.power.s4) << " AoAc=" << boolean(a.power.modern_standby) << " hiberfile=" << boolean(a.power.hiber_file)
            << " wake-alarm=" << boolean(a.power.wake_alarm) << '\n';
    }
    if (!a.warnings.empty()) {
        out << "\nWarnings\n";
        const size_t limit = (std::min)(a.warnings.size(), size_t{10});
        for (size_t i = 0; i < limit; ++i) out << "  " << safe(a.warnings[i]) << '\n';
        if (a.warnings.size() > limit) out << "  " << a.warnings.size() - limit << " additional warnings; use --json.\n";
    }
    out << "\nLimitations\n";
    for (const auto& l : a.limitations) out << "  " << safe(l) << '\n';
    return out.str();
}
}
