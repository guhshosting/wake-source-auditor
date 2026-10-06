#include "wsa/model.hpp"
namespace wsa {
std::string explain_wake(const WakeEvent& event) {
    const auto text = event.fields.find("WakeSourceText");
    if (text == event.fields.end() || text->second.empty())
        return "Windows recorded a resume event without a usable wake-source description; cause unknown.";
    if (text->second == "Unknown" || text->second == "unknown") return "Windows reports an unknown wake source.";
    return "Windows event reports wake source: " + text->second + ". This is the event's report, not an independently verified cause.";
}
std::string explain_power(const Power& power) {
    if (power.source.status != "ok") return "Power capabilities unavailable.";
    if (power.modern_standby) return "Modern Standby (AoAc) capability reported; traditional resume events may not describe all activity.";
    if (power.s1 || power.s2 || power.s3) return "Traditional standby capability reported.";
    if (power.s4) return "Hibernation capability reported, but no traditional standby or Modern Standby capability.";
    return "Windows reports no standby, Modern Standby, or hibernation capability in this environment.";
}
}
