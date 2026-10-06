#include "wsa/model.hpp"
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <string_view>
namespace {
bool output(const std::string& text, DWORD stream) {
    HANDLE handle = GetStdHandle(stream); DWORD mode = 0;
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return false;
    if (GetConsoleMode(handle, &mode)) {
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (count == 0) return false;
        std::wstring wide(static_cast<size_t>(count), L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), count)) return false;
        size_t offset = 0;
        while (offset < wide.size()) {
            DWORD written = 0;
            const auto chunk = static_cast<DWORD>((std::min)(wide.size() - offset, size_t{16384}));
            if (!WriteConsoleW(handle, wide.data() + offset, chunk, &written, nullptr) || written == 0) return false;
            offset += written;
        }
    } else {
        size_t offset = 0;
        while (offset < text.size()) {
            DWORD written = 0;
            const auto chunk = static_cast<DWORD>((std::min)(text.size() - offset, size_t{65536}));
            if (!WriteFile(handle, text.data() + offset, chunk, &written, nullptr) || written == 0) return false;
            offset += written;
        }
    }
    return true;
}
constexpr const char* help =
    "wake-source-auditor 1.0.0\nRead-only local Windows wake audit. No network or auto-elevation.\n\n"
    "Usage: wake-source-auditor [--json] [--last-wake | --devices | --timers]\n"
    "       wake-source-auditor --help | --version\n\n"
    "Default: all sources. Focus switches may be combined.\n"
    "--last-wake  Bounded System log evidence (30 days, at most 32 events).\n"
    "--devices    Wake capability and separately reported enabled devices.\n"
    "--timers     WakeToRun tasks; active kernel wake timers are unavailable.\n"
    "--json       UTF-8 JSON schema 1.0, diagnostics included in JSON.\n"
    "Exit codes: 0 completed (including absent evidence), 2 usage,\n"
    "3 required power-capability acquisition failed, 4 internal/output error.\n";
}
int wmain(int argc, wchar_t** argv) {
    try {
        wsa::Options options;
        bool focused = false, show_help = false, show_version = false;
        for (int i = 1; i < argc; ++i) {
            const std::wstring_view arg(argv[i]);
            if (arg == L"--help") show_help = true;
            else if (arg == L"--version") show_version = true;
            else if (arg == L"--json") options.json = true;
            else if (arg == L"--last-wake" || arg == L"--devices" || arg == L"--timers") {
                if (!focused) { options.power = options.events = options.devices = options.tasks = false; focused = true; }
                if (arg == L"--last-wake") options.events = true;
                if (arg == L"--devices") options.devices = true;
                if (arg == L"--timers") options.tasks = true;
            } else { output("Unknown argument. Use --help.\n", STD_ERROR_HANDLE); return 2; }
        }
        if ((show_help || show_version) && (argc != 2)) { output("--help and --version must be used alone.\n", STD_ERROR_HANDLE); return 2; }
        if (show_help) return output(help, STD_OUTPUT_HANDLE) ? 0 : 4;
        if (show_version) return output("wake-source-auditor 1.0.0\n", STD_OUTPUT_HANDLE) ? 0 : 4;
        const auto audit = wsa::collect(options);
        if (!output(options.json ? wsa::json(audit) : wsa::human(audit), STD_OUTPUT_HANDLE)) return 4;
        return audit.required_failure ? 3 : 0;
    } catch (const std::exception&) {
        output("Unexpected internal failure while auditing local Windows state.\n", STD_ERROR_HANDLE); return 4;
    } catch (...) {
        output("Unexpected internal failure.\n", STD_ERROR_HANDLE); return 4;
    }
}
