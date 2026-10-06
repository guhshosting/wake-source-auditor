#include "wsa/model.hpp"
#include <windows.h>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace wsa {
std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    if (value.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) throw std::length_error("Unicode input too large");
    const auto count = static_cast<int>(value.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), count, nullptr, 0, nullptr, nullptr);
    if (bytes == 0) throw std::runtime_error("Invalid UTF-16 from Windows");
    std::string result(static_cast<size_t>(bytes), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), count, result.data(), bytes, nullptr, nullptr) != bytes)
        throw std::runtime_error("UTF-8 conversion failed");
    return result;
}
std::string error_code(const std::string& operation, unsigned long code) {
    std::ostringstream out;
    out << operation << " failed (Windows/HRESULT code 0x" << std::hex << code << ").";
    return out.str();
}
Audit collect(const Options& options) {
    Audit result;
    result.limitations = {
        "Absence of evidence is not proof that no wake occurred.",
        "Most recent wake means the newest usable Power-Troubleshooter event 1 in the local System log within 30 days (at most 32 matching events inspected). Logs may be missing, inaccessible, cleared, or incomplete.",
        "Active kernel wake timers are unavailable: no documented user-mode enumeration contract is used. WakeToRun tasks are configured behavior, not active timers or proof of causality.",
        "Device capability and enabled lists are separate observations; friendly names are not unique and are never joined to infer configuration.",
        "Hardware, firmware, Modern Standby, virtualization, and Windows policy affect actual sleep and wake behavior; capabilities do not prove a successful physical wake."
    };
    if (options.power) acquire_power(result);
    if (options.events) acquire_events(result);
    if (options.devices) acquire_devices(result);
    if (options.tasks) acquire_tasks(result);
    return result;
}
}
