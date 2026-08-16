#include "AllowedApps.h"

#include <Psapi.h>

#include <algorithm>

std::wstring AllowedApps::processNameForWindow(HWND window) const {
    if (window == nullptr) {
        return L"";
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == 0) {
        return L"";
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (process == nullptr) {
        process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    }
    if (process == nullptr) {
        return L"";
    }

    wchar_t path[MAX_PATH]{};
    std::wstring result;
    if (GetModuleFileNameExW(process, nullptr, path, MAX_PATH) > 0) {
        result = fileNameFromPath(path);
    }
    CloseHandle(process);
    return toLower(result);
}

std::wstring AllowedApps::foregroundProcessName() const {
    return processNameForWindow(GetForegroundWindow());
}

bool AllowedApps::isWindowAllowed(HWND window) const {
    const std::wstring processName = processNameForWindow(window);
    if (processName.empty()) {
        return false;
    }

    return std::find(config_.allowedProcesses.begin(), config_.allowedProcesses.end(), processName) != config_.allowedProcesses.end();
}

bool AllowedApps::isForegroundAllowed() const {
    return isWindowAllowed(GetForegroundWindow());
}
