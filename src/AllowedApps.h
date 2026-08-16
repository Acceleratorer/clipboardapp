#pragma once

#include "AppState.h"

#include <Windows.h>

#include <string>

class AllowedApps {
public:
    explicit AllowedApps(const AppConfig& config) : config_(config) {}

    bool isForegroundAllowed() const;
    bool isWindowAllowed(HWND window) const;
    std::wstring foregroundProcessName() const;
    std::wstring processNameForWindow(HWND window) const;

private:
    const AppConfig& config_;
};
