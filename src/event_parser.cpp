#include "wsa/model.hpp"
#include "wsa/raii.hpp"
#include <xmllite.h>
#include <shlwapi.h>
#include <set>
namespace wsa {
namespace {
bool utc_timestamp(const std::string& text) {
    if (text.size() < 20 || text.size() > 30 || text.back() != 'Z' || text[4] != '-' || text[7] != '-' ||
        text[10] != 'T' || text[13] != ':' || text[16] != ':') return false;
    if (text.size() > 20) {
        if (text[19] != '.' || text.size() < 22) return false;
        for (size_t i = 20; i + 1 < text.size(); ++i) if (text[i] < '0' || text[i] > '9') return false;
    }
    auto number = [&text](size_t offset, size_t length) -> WORD {
        unsigned value = 0;
        for (size_t i = offset; i < offset + length; ++i) {
            if (text[i] < '0' || text[i] > '9') return 0xffff;
            value = value * 10U + static_cast<unsigned>(text[i] - '0');
        }
        return static_cast<WORD>(value);
    };
    SYSTEMTIME time{};
    time.wYear = number(0, 4); time.wMonth = number(5, 2); time.wDay = number(8, 2);
    time.wHour = number(11, 2); time.wMinute = number(14, 2); time.wSecond = number(17, 2);
    FILETIME file{};
    return SystemTimeToFileTime(&time, &file) != FALSE;
}
std::wstring attribute(IXmlReader* reader, const wchar_t* name) {
    if (reader->MoveToAttributeByName(name, nullptr) != S_OK) return {};
    const wchar_t* value = nullptr; UINT length = 0;
    const HRESULT hr = reader->GetValue(&value, &length);
    std::wstring result = SUCCEEDED(hr) && value ? std::wstring(value, length) : std::wstring();
    reader->MoveToElement();
    return result;
}
}
std::optional<WakeEvent> parse_wake_xml(const std::wstring& xml, std::string& error) {
    error.clear();
    if (xml.empty() || xml.size() > size_t{512} * 1024) { error = "Event XML is empty or exceeds 1 MiB."; return {}; }
    // UTF-16LE with BOM; XmlLite does not fetch external resources and DTDs are prohibited.
    std::wstring input = L"\uFEFF" + xml;
    ComPtr<IStream> stream;
    *stream.put() = SHCreateMemStream(reinterpret_cast<const BYTE*>(input.data()), static_cast<UINT>(input.size() * sizeof(wchar_t)));
    ComPtr<IXmlReader> reader;
    if (!stream.get() || FAILED(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.put()), nullptr))) {
        error = "Cannot initialize XML reader."; return {};
    }
    if (FAILED(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit)) ||
        FAILED(reader->SetProperty(XmlReaderProperty_MaxElementDepth, 32)) || FAILED(reader->SetInput(stream.get()))) {
        error = "Cannot configure safe XML reader."; return {};
    }
    WakeEvent event;
    std::vector<std::wstring> path;
    std::wstring current_field;
    const std::set<std::wstring> fields{L"SleepTime", L"WakeTime", L"WakeSourceType", L"WakeSourceText", L"WakeTimerOwner", L"WakeTimerContext"};
    XmlNodeType type{};
    HRESULT hr = S_OK;
    bool root_seen = false;
    std::set<std::wstring> system_fields;
    while ((hr = reader->Read(&type)) == S_OK) {
        const wchar_t* raw = nullptr; UINT length = 0;
        if (type == XmlNodeType_Element) {
            if (FAILED(reader->GetLocalName(&raw, &length)) || !raw) { error = "Missing XML name."; return {}; }
            std::wstring name(raw, length);
            if (path.empty()) {
                const wchar_t* uri = nullptr; UINT uri_length = 0;
                if (root_seen || name != L"Event" || FAILED(reader->GetNamespaceUri(&uri, &uri_length)) || !uri ||
                    std::wstring(uri, uri_length) != L"http://schemas.microsoft.com/win/2004/08/events/event") {
                    error = "Not a Windows Event document."; return {};
                }
                root_seen = true;
            }
            if (!current_field.empty()) { error = "Unexpected nested wake-source data."; return {}; }
            path.push_back(name);
            if (path.size() == 3 && path[1] == L"System") {
                if (!system_fields.insert(name).second) { error = "Duplicate System field."; return {}; }
                if (name == L"Provider") event.provider = utf8(attribute(reader.get(), L"Name"));
                if (name == L"TimeCreated") event.timestamp = utf8(attribute(reader.get(), L"SystemTime"));
            }
            current_field.clear();
            if (path.size() == 3 && path[1] == L"EventData" && name == L"Data") {
                auto key = attribute(reader.get(), L"Name");
                if (fields.count(key)) {
                    if (event.fields.count(utf8(key))) { error = "Duplicate wake-source field."; return {}; }
                    current_field = std::move(key);
                    event.fields.emplace(utf8(current_field), "");
                }
            }
            if (reader->IsEmptyElement()) { path.pop_back(); current_field.clear(); }
        } else if (type == XmlNodeType_EndElement) {
            if (!path.empty()) path.pop_back();
            current_field.clear();
        } else if (type == XmlNodeType_Text || type == XmlNodeType_CDATA || type == XmlNodeType_Whitespace) {
            if (FAILED(reader->GetValue(&raw, &length)) || !raw) continue;
            const auto value = utf8(std::wstring(raw, length));
            if (path.size() == 3 && path[1] == L"System") {
                if (path[2] == L"EventID") event.event_id += value;
                if (path[2] == L"EventRecordID") event.record_id += value;
            }
            if (!current_field.empty() && path.size() == 3) event.fields[utf8(current_field)] += value;
        }
    }
    if (FAILED(hr) || !root_seen || !path.empty() || event.provider != "Microsoft-Windows-Power-Troubleshooter" || event.event_id != "1" || !utc_timestamp(event.timestamp)) {
        error = "Malformed or unexpected Power-Troubleshooter event (provider, event ID, or timestamp)."; return {};
    }
    event.explanation = explain_wake(event);
    return event;
}
}
