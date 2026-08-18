#pragma once

#include "AppState.h"
#include "ClipboardLibrary.h"

#include <Windows.h>

#include <string>

struct TextConversionChoice {
    bool handled;
    bool convertToFile;
    std::wstring fileNamePrefix;

    TextConversionChoice() : handled(false), convertToFile(false) {}
};

struct ClipboardPickResult {
    int selectedIndex;
    std::wstring selectedFileName;
    std::wstring selectedText;
    bool sendTextAsFile;
    bool multiText;
    bool extractTextFromFile;

    ClipboardPickResult() : selectedIndex(-1), sendTextAsFile(true), multiText(false), extractTextFromFile(false) {}
};

class Ui {
public:
    Ui(HINSTANCE instance, HWND owner, AppState& state, ClipboardLibrary& library);

    bool addTrayIcon();
    void removeTrayIcon();
    void showTrayMenu();
    void showSettings();
    TextConversionChoice askTextConversion(size_t characterCount, const std::wstring& defaultFileName);
    void showInfo(const std::wstring& message);
    ClipboardPickResult chooseClipboardItem(bool sendTextAsFileByDefault = true);
    AppState& appState() { return state_; }
    ClipboardLibrary& library() { return library_; }

private:
    HINSTANCE instance_;
    HWND owner_;
    AppState& state_;
    ClipboardLibrary& library_;
};
