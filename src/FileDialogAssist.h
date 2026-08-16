#pragma once

#include "AllowedApps.h"
#include "ClipboardLibrary.h"
#include "TextFileConverter.h"

#include <Windows.h>

#include <string>

class ClipboardMonitor;
struct ClipboardPickResult;
class Ui;

class FileDialogAssist {
public:
    FileDialogAssist(const AppConfig& config, const AllowedApps& allowedApps, ClipboardLibrary& library, TextFileConverter& converter, ClipboardMonitor& clipboard, Ui& ui);

    void handleHotkey();
    void checkAutoDetect();

private:
    void assistActiveDialog(bool sendTextAsFileByDefault);
    void pasteIntoActiveInput(const ClipboardPickResult& pick, const ClipboardItem& item, HWND targetWindow);
    HWND findOpenDialog() const;
    bool fillDialogWithPath(HWND dialog, const std::wstring& path) const;
    std::wstring pathForItem(const ClipboardItem& item, const std::wstring& selectedFileName);

    const AppConfig& config_;
    const AllowedApps& allowedApps_;
    ClipboardLibrary& library_;
    TextFileConverter& converter_;
    ClipboardMonitor& clipboard_;
    Ui& ui_;
    HWND lastPromptedDialog_;
};
