#include "wsa/model.hpp"
#include "wsa/raii.hpp"
namespace wsa {
void acquire_events(Audit& audit) {
    EventHandle query(EvtQuery(nullptr, L"System",
        L"*[System[Provider[@Name='Microsoft-Windows-Power-Troubleshooter'] and EventID=1 and TimeCreated[timediff(@SystemTime) <= 2592000000]]]",
        static_cast<DWORD>(EvtQueryChannelPath) | static_cast<DWORD>(EvtQueryReverseDirection)));
    if (!query.get()) {
        audit.events.status = "unavailable";
        audit.warnings.push_back(error_code("EvtQuery(System)", GetLastError())); return;
    }
    audit.events.status = "ok";
    for (unsigned i = 0; i < 32; ++i) {
        EVT_HANDLE raw = nullptr; DWORD count = 0;
        if (!EvtNext(query.get(), 1, &raw, 2000, 0, &count)) {
            const auto code = GetLastError();
            if (code != ERROR_NO_MORE_ITEMS) { audit.events.status = "partial"; audit.warnings.push_back(error_code("EvtNext", code)); }
            return;
        }
        EventHandle event(raw);
        DWORD needed = 0, properties = 0;
        EvtRender(nullptr, event.get(), EvtRenderEventXml, 0, nullptr, &needed, &properties);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || needed == 0 || needed > 1024 * 1024) {
            audit.events.status = "partial"; audit.warnings.emplace_back("Event XML unavailable or exceeds 1 MiB; skipped."); continue;
        }
        std::vector<wchar_t> buffer((needed + sizeof(wchar_t) - 1) / sizeof(wchar_t), L'\0');
        if (!EvtRender(nullptr, event.get(), EvtRenderEventXml, needed, buffer.data(), &needed, &properties)) {
            audit.events.status = "partial"; audit.warnings.push_back(error_code("EvtRender", GetLastError())); continue;
        }
        std::string error;
        auto parsed = parse_wake_xml(std::wstring(buffer.data()), error);
        if (!parsed) { audit.events.status = "partial"; audit.warnings.push_back(error); continue; }
        audit.last_wake = std::move(parsed); return;
    }
    audit.events.status = "partial";
    audit.warnings.emplace_back("Reached the 32-event inspection limit without usable wake evidence.");
}
}
