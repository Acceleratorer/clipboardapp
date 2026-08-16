#pragma once

#include "AllowedApps.h"
#include "ClipboardLibrary.h"
#include "TextFileConverter.h"

#include <Windows.h>

#include <string>
#include <vector>

class Ui;

class ClipboardMonitor {
public:
    ClipboardMonitor(HWND messageWindow, const AppConfig& config, const AllowedApps& allowedApps, ClipboardLibrary& library, TextFileConverter& converter, Ui& ui);

    bool start();
    void stop();
    void handleClipboardUpdate();

    bool putFilesOnClipboard(const std::vector<std::wstring>& files) const;
    bool putTextOnClipboard(const std::wstring& text) const;
    void sendPasteToWindow(HWND targetWindow);
    bool shouldInterceptPasteShortcut(HWND targetWindow);
    bool prepareLongTextForPaste(HWND targetWindow, bool invokedByShortcut);
    bool supportsFilePaste(HWND targetWindow) const;
    std::wstring readClipboardText() const;
    std::vector<std::wstring> readClipboardFiles() const;
    bool cacheClipboardImage();
    bool cacheClipboardPng(UINT format);
    bool cacheClipboardDib();
    bool cacheClipboardBitmap();
    std::vector<std::wstring> cacheFiles(const std::vector<std::wstring>& files, std::vector<std::wstring>& cachedOriginalFiles) const;
    HWND messageWindow() const { return messageWindow_; }
    HWND nextClipboardViewer() const { return nextClipboardViewer_; }
    bool shouldSkipPasteShortcut() const { return skipNextPasteShortcut_; }
    void consumePasteShortcutSkip() { skipNextPasteShortcut_ = false; }

private:
    HWND messageWindow_;
    const AppConfig& config_;
    const AllowedApps& allowedApps_;
    ClipboardLibrary& library_;
    TextFileConverter& converter_;
    Ui& ui_;
    HWND nextClipboardViewer_;
    bool skipNextPasteShortcut_;
    std::wstring declinedTextSignature_;
    std::wstring preparedTextSignature_;
    HWND preparedTargetWindow_;
    bool preparingClipboard_;
};
