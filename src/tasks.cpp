#include "wsa/model.hpp"
#include "wsa/raii.hpp"
#include <taskschd.h>
#include <chrono>
#include <cstdio>
namespace wsa {
namespace {
struct Apartment {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~Apartment() { if (SUCCEEDED(result)) CoUninitialize(); }
};
std::optional<std::string> date_local(DATE date) {
    SYSTEMTIME time{};
    if (date == 0 || !VariantTimeToSystemTime(date, &time)) return {};
    char text[32]{};
    std::snprintf(text, sizeof(text), "%04u-%02u-%02uT%02u:%02u:%02u", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    return std::string(text);
}
struct Folder { ComPtr<ITaskFolder> value; unsigned depth = 0; };
}
void acquire_tasks(Audit& audit) {
    audit.tasks_source.status = "ok";
    auto failure = [&audit](const char* operation, HRESULT hr) {
        audit.tasks_source.status = "partial";
        if (audit.warnings.size() < 64) audit.warnings.push_back(error_code(operation, static_cast<unsigned long>(hr)));
    };
    Apartment apartment;
    if (FAILED(apartment.result)) { failure("CoInitializeEx", apartment.result); audit.tasks_source.status = "unavailable"; return; }
    // Process-local COM authentication/impersonation defaults, as required by the
    // documented Task Scheduler sample; this does not modify any system setting.
    HRESULT hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        failure("CoInitializeSecurity", hr); audit.tasks_source.status = "unavailable"; return;
    }
    if (hr == RPC_E_TOO_LATE) {
        audit.tasks_source.status = "partial";
        audit.warnings.emplace_back("COM security was already initialized; task access depends on existing process defaults.");
    }
    ComPtr<ITaskService> service;
    hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void**>(service.put()));
    if (FAILED(hr)) { failure("CoCreateInstance(TaskScheduler)", hr); audit.tasks_source.status = "unavailable"; return; }
    VARIANT empty{}; VariantInit(&empty);
    // Empty server/credentials means local computer and current user's credentials.
    hr = service->Connect(empty, empty, empty, empty);
    if (FAILED(hr)) { failure("ITaskService::Connect(local)", hr); audit.tasks_source.status = "unavailable"; return; }
    Bstr root_path; *root_path.put() = SysAllocString(L"\\");
    if (root_path.str().empty()) throw std::bad_alloc();
    Folder root;
    hr = service->GetFolder(root_path.get(), root.value.put());
    if (FAILED(hr)) { failure("GetFolder(root)", hr); audit.tasks_source.status = "unavailable"; return; }
    std::vector<Folder> pending;
    pending.push_back(std::move(root));
    unsigned folders = 0, tasks = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!pending.empty()) {
        if (++folders > 256 || tasks >= 4096 || std::chrono::steady_clock::now() > deadline) {
            audit.tasks_source.status = "partial"; audit.warnings.emplace_back("Task enumeration reached its folder/task/time budget (256 folders, 4096 tasks, 15 seconds between calls)."); return;
        }
        Folder folder = std::move(pending.back()); pending.pop_back();
        ComPtr<IRegisteredTaskCollection> collection;
        hr = folder.value->GetTasks(TASK_ENUM_HIDDEN, collection.put());
        if (FAILED(hr)) failure("GetTasks", hr);
        else {
            LONG count = 0;
            hr = collection->get_Count(&count);
            if (FAILED(hr)) failure("Task count", hr);
            for (LONG i = 0; i < count; ++i) {
                if (tasks++ >= 4096 || std::chrono::steady_clock::now() > deadline) {
                    audit.tasks_source.status = "partial"; audit.warnings.emplace_back("Task enumeration reached its task/time budget."); return;
                }
                VARIANT index{}; index.vt = VT_I4; index.lVal = i + 1;
                ComPtr<IRegisteredTask> task;
                hr = collection->get_Item(index, task.put());
                if (FAILED(hr)) { failure("Task item", hr); continue; }
                ComPtr<ITaskDefinition> definition;
                hr = task->get_Definition(definition.put());
                if (FAILED(hr)) { failure("Task definition", hr); continue; }
                ComPtr<ITaskSettings> settings;
                hr = definition->get_Settings(settings.put());
                if (FAILED(hr)) { failure("Task settings", hr); continue; }
                VARIANT_BOOL wake = VARIANT_FALSE;
                hr = settings->get_WakeToRun(&wake);
                if (FAILED(hr)) { failure("WakeToRun", hr); continue; }
                if (wake == VARIANT_FALSE) continue;
                Task item; Bstr path;
                hr = task->get_Path(path.put());
                if (FAILED(hr)) { failure("Task path", hr); continue; }
                item.path = utf8(path.str());
                VARIANT_BOOL enabled = VARIANT_FALSE;
                hr = task->get_Enabled(&enabled);
                if (FAILED(hr)) { failure("Task enabled state", hr); continue; }
                item.enabled = enabled != VARIANT_FALSE;
                DATE next = 0;
                hr = task->get_NextRunTime(&next);
                if (FAILED(hr)) failure("Task next run", hr);
                else item.next_run_local = date_local(next);
                audit.tasks.push_back(std::move(item));
            }
        }
        ComPtr<ITaskFolderCollection> children;
        hr = folder.value->GetFolders(0, children.put());
        if (FAILED(hr)) { failure("GetFolders", hr); continue; }
        LONG count = 0; hr = children->get_Count(&count);
        if (FAILED(hr)) { failure("Folder count", hr); continue; }
        if (folder.depth >= 16 && count > 0) {
            audit.tasks_source.status = "partial"; audit.warnings.emplace_back("Task folder depth capped at 16."); continue;
        }
        for (LONG i = 0; i < count; ++i) {
            if (pending.size() + folders >= 256) { audit.tasks_source.status = "partial"; audit.warnings.emplace_back("Task folder count capped at 256."); break; }
            VARIANT index{}; index.vt = VT_I4; index.lVal = i + 1;
            Folder child; child.depth = folder.depth + 1;
            hr = children->get_Item(index, child.value.put());
            if (FAILED(hr)) failure("Folder item", hr);
            else pending.push_back(std::move(child));
        }
    }
}
}
