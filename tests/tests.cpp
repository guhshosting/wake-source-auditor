#include "wsa/model.hpp"
#include <windows.h>
#include <sddl.h>
#include <cstdio>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks = 0;
void check(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
std::wstring event(const std::wstring& fields) {
    return L"<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='Microsoft-Windows-Power-Troubleshooter'/><EventID>1</EventID><TimeCreated SystemTime='2026-10-01T12:00:00.000Z'/><EventRecordID>42</EventRecordID></System><EventData>" + fields + L"</EventData></Event>";
}
void synthetic() {
    std::string error;
    auto known = wsa::parse_wake_xml(event(L"<Data Name='WakeSourceText'>Power Button</Data><Data Name='WakeTime'>2026-10-01T11:59:59Z</Data>"), error);
    check(known.has_value(), "known event parsed");
    if (!known) throw std::runtime_error("known event missing");
    check(known.value().provider == "Microsoft-Windows-Power-Troubleshooter" && known.value().event_id == "1" && known.value().record_id == "42", "event identity");
    check(known.value().explanation.find("Power Button") != std::string::npos, "reported source retained");
    auto unknown = wsa::parse_wake_xml(event(L"<Data Name='WakeSourceText'>Unknown</Data><Data Name='WakeSourceType'>0</Data>"), error);
    check(unknown && unknown->explanation == "Windows reports an unknown wake source.", "unknown cause");
    auto device = wsa::parse_wake_xml(event(L"<Data Name='WakeSourceText'>Device - 鍵盤 &amp; Мышь</Data>"), error);
    check(device && device->fields.at("WakeSourceText") == u8"Device - 鍵盤 & Мышь", "Unicode and entity parsing");
    auto timer = wsa::parse_wake_xml(event(L"<Data Name='WakeSourceText'>Timer</Data><Data Name='WakeTimerOwner'>service.exe</Data><Data Name='WakeTimerContext'>Maintenance</Data>"), error);
    check(timer && timer->fields.at("WakeTimerOwner") == "service.exe", "timer owner");
    if (!timer) throw std::runtime_error("timer event missing");
    check(timer.value().explanation.find("Maintenance caused") == std::string::npos, "no invented task cause");
    auto missing = wsa::parse_wake_xml(event(L"<Data Name='WakeSourceType'>999</Data>"), error);
    check(missing && missing->explanation.find("cause unknown") != std::string::npos, "missing source text and unknown numeric type");
    check(!wsa::parse_wake_xml(L"<Event>", error), "malformed XML rejected");
    check(!wsa::parse_wake_xml(L"<!DOCTYPE Event [<!ENTITY x SYSTEM 'https://example.invalid/'>]>" + event(L"<Data Name='WakeSourceText'>&x;</Data>"), error), "DTD and external entities rejected");
    check(!wsa::parse_wake_xml(event(L"<Data Name='WakeSourceText'>a</Data><Data Name='WakeSourceText'>b</Data>"), error), "duplicate data rejected");
    auto wrong = event(L""); wrong.replace(wrong.find(L"<EventID>1"), 10, L"<EventID>2");
    check(!wsa::parse_wake_xml(wrong, error), "wrong event ID rejected");
    check(!wsa::parse_wake_xml(std::wstring(512 * 1024 + 1, L'x'), error), "oversize event rejected");
    auto invalid_time = event(L"");
    invalid_time.replace(invalid_time.find(L"2026-10-01T12:00:00.000Z"), 24, L"2026-99-01T12:00:00.000Z");
    check(!wsa::parse_wake_xml(invalid_time, error), "invalid event timestamp rejected");
    check(!wsa::parse_wake_xml(event(L"<Data Name='WakeSourceText'><x>Device</x></Data>"), error), "nested data rejected");
    check(!wsa::parse_wake_xml(L"<Event><System/></Event>", error), "missing namespace rejected");
    wsa::Audit audit;
    audit.events.status = "ok";
    check(wsa::human(audit).find("No usable wake evidence") != std::string::npos, "no history");
    audit.power.source.status = "ok";
    check(wsa::explain_power(audit.power).find("no standby") != std::string::npos, "no sleep capability");
    audit.power.modern_standby = true;
    check(wsa::explain_power(audit.power).find("Modern Standby") != std::string::npos, "Modern Standby");
    audit.power.modern_standby = false; audit.power.s3 = true;
    check(wsa::explain_power(audit.power).find("Traditional standby") != std::string::npos, "traditional standby");
    audit.power.s3 = false; audit.power.s4 = true;
    check(wsa::explain_power(audit.power).find("Hibernation capability") != std::string::npos, "hibernation only");
    wsa::Device capable;
    capable.identity = "id"; capable.name = u8"鍵盤"; capable.category = "configured_capability"; capable.wake_capable = true;
    wsa::Device enabled;
    enabled.name = capable.name; enabled.identity = capable.name; enabled.category = "configured_wake_behavior"; enabled.wake_enabled = true;
    audit.devices = {capable, enabled};
    audit.tasks.push_back({u8"\\维护\\Réveil", true, "2026-10-02T08:00:00"});
    audit.tasks.push_back({"\\DisabledWakeTask", false, {}});
    audit.last_wake = std::move(device);
    if (!audit.last_wake) throw std::runtime_error("device event missing");
    auto serialized = wsa::json(audit);
    check(serialized.find("\"wake_capable\":true,\"wake_enabled\":null") != std::string::npos, "capability is not configured state");
    check(serialized.find("\"wake_capable\":null,\"wake_enabled\":true") != std::string::npos, "enabled observation distinct");
    check(serialized.find("\"proves_wake\":false") != std::string::npos, "WakeToRun not causal proof");
    check(serialized.find("\"wake_to_run\":true,\"enabled\":false") != std::string::npos, "disabled WakeToRun task retained");
    check(serialized.find(u8"维护") != std::string::npos, "Unicode task JSON");
    check(wsa::utf8(L"\U0001F680鍵盤") == u8"🚀鍵盤", "surrogate pair conversion");
    check(wsa::quote_json("\"\\\n\t\r\b\f\x01") == "\"\\\"\\\\\\u000a\\u0009\\u000d\\u0008\\u000c\\u0001\"", "JSON escaping");
    audit.power.source.status = "unavailable";
    check(wsa::json(audit).find("\"s1\":null") != std::string::npos, "unavailable power is not false");
    audit.last_wake.value().explanation = "unsafe\x1b[31m\ntext";
    check(wsa::human(audit).find('\x1b') == std::string::npos, "terminal escape sanitized");
    audit.last_wake->explanation = std::string(511, 'x') + u8"鍵盤";
    const auto shortened = wsa::human(audit);
    check(shortened.find(std::string(511, 'x') + "... [truncated;") != std::string::npos &&
        shortened.find(std::string(511, 'x') + u8"鍵盤") == std::string::npos, "human truncation preserves UTF-8 boundary");
    audit.required_failure = true;
    check(wsa::json(audit).find("required_source_failure") != std::string::npos, "required failure modeled");
}
void integration() {
    const auto audit = wsa::collect({});
    check(audit.power.source.status == "ok", "real power acquisition");
    check(audit.events.status != "not_requested", "real bounded log query attempted");
    check(audit.devices_source.status != "not_requested", "real device acquisition attempted");
    check(audit.tasks_source.status != "not_requested", "real task acquisition attempted");
    check(!audit.required_failure, "audit completed");
    std::cout << wsa::json(audit);
}
void restricted_integration() {
    PSID admin = nullptr;
    check(ConvertStringSidToSidW(L"S-1-5-32-544", &admin) != FALSE, "admin SID");
    BOOL member = TRUE;
    const BOOL queried = CheckTokenMembership(nullptr, admin, &member);
    LocalFree(admin);
    check(queried && !member, "restricted child must not have enabled Administrators membership");
    HANDLE token = nullptr;
    check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) != FALSE, "restricted token query");
    DWORD bytes = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &bytes);
    std::vector<BYTE> buffer(bytes);
    const BOOL got = GetTokenInformation(token, TokenIntegrityLevel, buffer.data(), bytes, &bytes);
    CloseHandle(token);
    check(got != FALSE, "integrity query");
    auto* label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer.data());
    const DWORD integrity = *GetSidSubAuthority(label->Label.Sid, *GetSidSubAuthorityCount(label->Label.Sid) - 1);
    check(integrity == SECURITY_MANDATORY_MEDIUM_RID, "medium integrity required");
    std::cout << "Non-admin token verified: Administrators disabled, medium integrity.\n";
    integration();
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--restricted-integration") restricted_integration();
        else if (argc == 2 && std::string(argv[1]) == "--integration") integration();
        else if (argc == 2 && std::string(argv[1]) == "--json-fixture") {
            wsa::Audit a; a.tasks.push_back({u8"\\维护\\Réveil🚀", true, {}});
            a.warnings.push_back("quote\" slash\\ newline\n control\x01");
            std::cout << wsa::json(a); return 0;
        } else synthetic();
        std::cout << "Passed " << checks << " checks.\n";
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
    catch (...) { std::fputs("Unexpected test failure\n", stderr); return 1; }
}
