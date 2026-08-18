#include "ClipboardMonitor.h"

#include "Ui.h"

#include <Shellapi.h>
#include <Shlobj.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

namespace {
UINT pngClipboardFormat() {
    return RegisterClipboardFormatW(L"PNG");
}

std::wstring textSignature(const std::wstring& text, HWND targetWindow) {
    std::wstringstream stream;
    stream << reinterpret_cast<size_t>(targetWindow) << L":" << text.size() << L":";
    size_t hash = 2166136261u;
    for (size_t index = 0; index < text.size(); ++index) {
        hash ^= static_cast<size_t>(text[index]);
        hash *= 16777619u;
    }
    stream << hash;
    return stream.str();
}

bool processCanPasteFiles(const std::wstring& processName) {
    static const wchar_t* names[] = {
        L"chrome.exe",
        L"msedge.exe",
        L"firefox.exe",
        L"brave.exe",
        L"opera.exe",
        L"discord.exe",
        L"discordptb.exe",
        L"discordcanary.exe",
        L"telegram.exe",
        L"slack.exe"
    };
    return std::find(names, names + sizeof(names) / sizeof(names[0]), processName) != names + sizeof(names) / sizeof(names[0]);
}

unsigned long long maxCacheFileBytes(const AppConfig& config) {
    if (config.maxFileSizeMB <= 0) {
        return ~0ULL;
    }
    return static_cast<unsigned long long>(config.maxFileSizeMB) * 1024ULL * 1024ULL;
}

int effectiveTextThreshold(const AppConfig& config) {
    return config.textThreshold < 1 ? 1 : config.textThreshold;
}

bool byteSizeFitsCacheLimit(unsigned long long size, const AppConfig& config) {
    return size <= maxCacheFileBytes(config);
}

bool fileFitsCacheLimit(const std::wstring& path, const AppConfig& config) {
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes)) {
        return false;
    }

    ULARGE_INTEGER size;
    size.LowPart = attributes.nFileSizeLow;
    size.HighPart = attributes.nFileSizeHigh;
    return byteSizeFitsCacheLimit(size.QuadPart, config);
}

void activatePasteTarget(HWND targetWindow) {
    if (targetWindow == nullptr || !IsWindow(targetWindow)) {
        return;
    }

    HWND rootWindow = GetAncestor(targetWindow, GA_ROOT);
    if (rootWindow == nullptr) {
        rootWindow = targetWindow;
    }

    const DWORD targetThread = GetWindowThreadProcessId(rootWindow, nullptr);
    GUITHREADINFO threadInfo;
    ZeroMemory(&threadInfo, sizeof(threadInfo));
    threadInfo.cbSize = sizeof(threadInfo);
    GetGUIThreadInfo(targetThread, &threadInfo);

    const DWORD currentThread = GetCurrentThreadId();
    const bool attached = targetThread != 0 && targetThread != currentThread &&
        AttachThreadInput(currentThread, targetThread, TRUE) != FALSE;

    if (IsIconic(rootWindow)) {
        ShowWindow(rootWindow, SW_RESTORE);
    }
    BringWindowToTop(rootWindow);
    SetForegroundWindow(rootWindow);

    if (threadInfo.hwndFocus != nullptr && IsWindow(threadInfo.hwndFocus) &&
        (threadInfo.hwndFocus == rootWindow || IsChild(rootWindow, threadInfo.hwndFocus))) {
        SetFocus(threadInfo.hwndFocus);
    }

    if (attached) {
        AttachThreadInput(currentThread, targetThread, FALSE);
    }
}

bool writeDibToBmpFile(const std::wstring& path, const void* dibData, size_t dibSize) {
    if (dibData == nullptr || dibSize < sizeof(BITMAPINFOHEADER)) {
        return false;
    }

    const BITMAPINFOHEADER* header = static_cast<const BITMAPINFOHEADER*>(dibData);
    DWORD colors = header->biClrUsed;
    if (colors == 0 && header->biBitCount <= 8) {
        colors = 1u << header->biBitCount;
    }

    DWORD extraMasks = 0;
    if (header->biCompression == BI_BITFIELDS && header->biSize == sizeof(BITMAPINFOHEADER)) {
        extraMasks = 3 * sizeof(DWORD);
    }

    BITMAPFILEHEADER fileHeader;
    ZeroMemory(&fileHeader, sizeof(fileHeader));
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + header->biSize + extraMasks + colors * sizeof(RGBQUAD));
    fileHeader.bfSize = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + dibSize);

    const size_t totalSize = sizeof(BITMAPFILEHEADER) + dibSize;
    std::vector<unsigned char> bytes(totalSize);
    memcpy(&bytes[0], &fileHeader, sizeof(fileHeader));
    memcpy(&bytes[sizeof(BITMAPFILEHEADER)], dibData, dibSize);
    return writeBinaryFile(path, &bytes[0], bytes.size());
}

bool writeBitmapToBmpFile(const std::wstring& path, HBITMAP bitmap, unsigned long long maxBytes) {
    if (bitmap == nullptr) {
        return false;
    }

    BITMAP bitmapInfo;
    if (GetObjectW(bitmap, sizeof(bitmapInfo), &bitmapInfo) == 0) {
        return false;
    }

    BITMAPINFOHEADER header;
    ZeroMemory(&header, sizeof(header));
    header.biSize = sizeof(BITMAPINFOHEADER);
    header.biWidth = bitmapInfo.bmWidth;
    header.biHeight = bitmapInfo.bmHeight;
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;

    const unsigned long long bitmapWidth = bitmapInfo.bmWidth < 0
        ? static_cast<unsigned long long>(-static_cast<long long>(bitmapInfo.bmWidth))
        : static_cast<unsigned long long>(bitmapInfo.bmWidth);
    const unsigned long long bitmapHeight = bitmapInfo.bmHeight < 0
        ? static_cast<unsigned long long>(-static_cast<long long>(bitmapInfo.bmHeight))
        : static_cast<unsigned long long>(bitmapInfo.bmHeight);
    const unsigned long long rowBytes64 = ((bitmapWidth * header.biBitCount + 31ULL) / 32ULL) * 4ULL;
    const unsigned long long imageBytes64 = rowBytes64 * bitmapHeight;
    const unsigned long long fileBytes64 = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageBytes64;
    if (fileBytes64 > maxBytes || imageBytes64 > MAXDWORD) {
        return false;
    }

    const DWORD imageBytes = static_cast<DWORD>(imageBytes64);
    header.biSizeImage = imageBytes;

    std::vector<unsigned char> pixels(imageBytes);
    BITMAPINFO dibInfo;
    ZeroMemory(&dibInfo, sizeof(dibInfo));
    dibInfo.bmiHeader = header;

    HDC dc = GetDC(nullptr);
    const int scanLines = GetDIBits(dc, bitmap, 0, static_cast<UINT>(bitmapInfo.bmHeight), &pixels[0], &dibInfo, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (scanLines == 0) {
        return false;
    }

    BITMAPFILEHEADER fileHeader;
    ZeroMemory(&fileHeader, sizeof(fileHeader));
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + imageBytes;

    std::vector<unsigned char> bytes(fileHeader.bfSize);
    memcpy(&bytes[0], &fileHeader, sizeof(fileHeader));
    memcpy(&bytes[sizeof(BITMAPFILEHEADER)], &header, sizeof(header));
    memcpy(&bytes[fileHeader.bfOffBits], &pixels[0], pixels.size());
    return writeBinaryFile(path, &bytes[0], bytes.size());
}
} // namespace

ClipboardMonitor::ClipboardMonitor(HWND messageWindow, const AppConfig& config, const AllowedApps& allowedApps, ClipboardLibrary& library, TextFileConverter& converter, Ui& ui)
    : messageWindow_(messageWindow), config_(config), allowedApps_(allowedApps), library_(library), converter_(converter), ui_(ui), nextClipboardViewer_(nullptr), skipNextPasteShortcut_(false), preparedTargetWindow_(nullptr), preparingClipboard_(false) {}

bool ClipboardMonitor::start() {
    nextClipboardViewer_ = SetClipboardViewer(messageWindow_);
    return true;
}

void ClipboardMonitor::stop() {
    ChangeClipboardChain(messageWindow_, nextClipboardViewer_);
    nextClipboardViewer_ = nullptr;
}

void ClipboardMonitor::handleClipboardUpdate() {
    if (preparingClipboard_) {
        return;
    }

    if (cacheClipboardImage()) {
        return;
    }

    const std::vector<std::wstring> files = readClipboardFiles();
    if (!files.empty()) {
        std::vector<std::wstring> cachedOriginalFiles;
        const std::vector<std::wstring> cachedFiles = cacheFiles(files, cachedOriginalFiles);
        if (!cachedFiles.empty()) {
            library_.addFiles(cachedFiles, cachedOriginalFiles);
        }
        return;
    }

    const std::wstring text = readClipboardText();
    if (text.empty()) {
        return;
    }

    library_.addText(text);
}

bool ClipboardMonitor::shouldInterceptPasteShortcut(HWND targetWindow) {
    if (targetWindow == nullptr || !supportsFilePaste(targetWindow)) {
        return false;
    }

    const std::wstring text = readClipboardText();
    if (text.empty() || static_cast<int>(text.size()) < effectiveTextThreshold(config_)) {
        return false;
    }

    return true;
}

bool ClipboardMonitor::supportsFilePaste(HWND targetWindow) const {
    if (!allowedApps_.isWindowAllowed(targetWindow)) {
        return false;
    }
    return processCanPasteFiles(allowedApps_.processNameForWindow(targetWindow));
}

bool ClipboardMonitor::prepareLongTextForPaste(HWND targetWindow, bool invokedByShortcut) {
    if (targetWindow == nullptr || !supportsFilePaste(targetWindow)) {
        return false;
    }

    const std::wstring text = readClipboardText();
    if (text.empty() || static_cast<int>(text.size()) < effectiveTextThreshold(config_)) {
        return false;
    }

    const std::wstring signature = textSignature(text, targetWindow);
    if (!invokedByShortcut && (signature == declinedTextSignature_ || (signature == preparedTextSignature_ && targetWindow == preparedTargetWindow_))) {
        return false;
    }

    TextConversionChoice choice;
    if (config_.askBeforeConverting) {
        choice = ui_.askTextConversion(text.size(), L"pasted_text");
        if (!choice.handled) {
            declinedTextSignature_ = signature;
            return invokedByShortcut;
        }
    } else {
        choice.handled = true;
        choice.convertToFile = true;
        choice.fileNamePrefix = L"pasted_text";
    }

    if (!choice.convertToFile) {
        declinedTextSignature_ = signature;
        if (invokedByShortcut) {
            skipNextPasteShortcut_ = true;
            SetForegroundWindow(targetWindow);
            INPUT inputs[4];
            ZeroMemory(inputs, sizeof(inputs));
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = VK_CONTROL;
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = 'V';
            inputs[2].type = INPUT_KEYBOARD;
            inputs[2].ki.wVk = 'V';
            inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
            inputs[3].type = INPUT_KEYBOARD;
            inputs[3].ki.wVk = VK_CONTROL;
            inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(4, inputs, sizeof(INPUT));
            return true;
        }
        return false;
    }

    const std::wstring generated = converter_.createTextFileWithName(text, choice.fileNamePrefix.empty() ? L"pasted_text" : choice.fileNamePrefix);
    if (generated.empty()) {
        return false;
    }

    std::vector<std::wstring> generatedFiles;
    generatedFiles.push_back(generated);
    preparingClipboard_ = true;
    const bool placed = putFilesOnClipboard(generatedFiles);
    preparingClipboard_ = false;
    if (!placed) {
        return false;
    }

    preparedTextSignature_ = signature;
    preparedTargetWindow_ = targetWindow;
    declinedTextSignature_.clear();

    if (invokedByShortcut) {
        sendPasteToWindow(targetWindow);
    }
    return true;
}

void ClipboardMonitor::sendPasteToWindow(HWND targetWindow) {
    activatePasteTarget(targetWindow);
    INPUT inputs[4];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    skipNextPasteShortcut_ = true;
    SendInput(4, inputs, sizeof(INPUT));
}

std::wstring ClipboardMonitor::readClipboardText() const {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        return L"";
    }
    if (!OpenClipboard(messageWindow_)) {
        return L"";
    }

    std::wstring result;
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle != nullptr) {
        const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(handle));
        if (text != nullptr) {
            result = text;
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    return result;
}

std::vector<std::wstring> ClipboardMonitor::readClipboardFiles() const {
    if (!IsClipboardFormatAvailable(CF_HDROP)) {
        return std::vector<std::wstring>();
    }
    if (!OpenClipboard(messageWindow_)) {
        return std::vector<std::wstring>();
    }

    std::vector<std::wstring> files;
    HDROP drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
    if (drop != nullptr) {
        const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT index = 0; index < count; ++index) {
            const UINT length = DragQueryFileW(drop, index, nullptr, 0);
            std::wstring path(length + 1, L'\0');
            DragQueryFileW(drop, index, &path[0], static_cast<UINT>(path.size()));
            path.resize(length);
            files.push_back(path);
        }
    }
    CloseClipboard();
    return files;
}

bool ClipboardMonitor::cacheClipboardImage() {
    const UINT pngFormat = pngClipboardFormat();
    if (pngFormat != 0 && IsClipboardFormatAvailable(pngFormat) && cacheClipboardPng(pngFormat)) {
        return true;
    }
    if (IsClipboardFormatAvailable(CF_DIB) && cacheClipboardDib()) {
        return true;
    }
    if (IsClipboardFormatAvailable(CF_BITMAP) && cacheClipboardBitmap()) {
        return true;
    }
    return false;
}

bool ClipboardMonitor::cacheClipboardPng(UINT format) {
    if (!OpenClipboard(messageWindow_)) {
        return false;
    }

    bool saved = false;
    HANDLE handle = GetClipboardData(format);
    if (handle != nullptr) {
        const SIZE_T size = GlobalSize(handle);
        const void* data = GlobalLock(handle);
        if (data != nullptr) {
            if (size > 0 && byteSizeFitsCacheLimit(static_cast<unsigned long long>(size), config_)) {
                const std::wstring imageDir = pathJoin(converter_.outputDir(), L"images");
                const std::wstring path = uniquePath(imageDir, L"clipboard_image", L".png");
                saved = writeBinaryFile(path, data, static_cast<size_t>(size));
                if (saved) {
                    library_.addImage(path, L"Image cache: " + fileNameFromPath(path));
                }
            }
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    return saved;
}

bool ClipboardMonitor::cacheClipboardDib() {
    if (!OpenClipboard(messageWindow_)) {
        return false;
    }

    bool saved = false;
    HANDLE handle = GetClipboardData(CF_DIB);
    if (handle != nullptr) {
        const SIZE_T size = GlobalSize(handle);
        const void* data = GlobalLock(handle);
        const unsigned long long fileSize = static_cast<unsigned long long>(sizeof(BITMAPFILEHEADER)) + static_cast<unsigned long long>(size);
        if (data != nullptr) {
            if (size > 0 && byteSizeFitsCacheLimit(fileSize, config_)) {
                const std::wstring imageDir = pathJoin(converter_.outputDir(), L"images");
                const std::wstring path = uniquePath(imageDir, L"clipboard_image", L".bmp");
                saved = writeDibToBmpFile(path, data, static_cast<size_t>(size));
                if (saved) {
                    library_.addImage(path, L"Image cache: " + fileNameFromPath(path));
                }
            }
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    return saved;
}

bool ClipboardMonitor::cacheClipboardBitmap() {
    if (!OpenClipboard(messageWindow_)) {
        return false;
    }

    bool saved = false;
    HBITMAP bitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    if (bitmap != nullptr) {
        const std::wstring imageDir = pathJoin(converter_.outputDir(), L"images");
        const std::wstring path = uniquePath(imageDir, L"clipboard_image", L".bmp");
        saved = writeBitmapToBmpFile(path, bitmap, maxCacheFileBytes(config_));
        if (saved) {
            library_.addImage(path, L"Image cache: " + fileNameFromPath(path));
        }
    }
    CloseClipboard();
    return saved;
}

std::vector<std::wstring> ClipboardMonitor::cacheFiles(const std::vector<std::wstring>& files, std::vector<std::wstring>& cachedOriginalFiles) const {
    std::vector<std::wstring> cached;
    cachedOriginalFiles.clear();
    const std::wstring fileDir = pathJoin(converter_.outputDir(), L"files");
    for (size_t index = 0; index < files.size(); ++index) {
        const std::wstring& source = files[index];
        if (!fileExists(source) || directoryExists(source) || !fileFitsCacheLimit(source, config_)) {
            continue;
        }

        std::wstring extension = extensionFromPath(source);
        if (extension.empty()) {
            extension = L".bin";
        }
        const std::wstring destination = uniquePath(fileDir, fileStemFromPath(source), extension);
        if (copyFileToPath(source, destination)) {
            cached.push_back(destination);
            cachedOriginalFiles.push_back(source);
        }
    }
    return cached;
}

bool ClipboardMonitor::putFilesOnClipboard(const std::vector<std::wstring>& files) const {
    if (files.empty()) {
        return false;
    }
    if (!OpenClipboard(messageWindow_)) {
        return false;
    }

    EmptyClipboard();

    size_t pathsBytes = sizeof(wchar_t);
    for (size_t index = 0; index < files.size(); ++index) {
        pathsBytes += (files[index].size() + 1) * sizeof(wchar_t);
    }

    const size_t totalBytes = sizeof(DROPFILES) + pathsBytes;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, totalBytes);
    if (memory == nullptr) {
        CloseClipboard();
        return false;
    }

    DROPFILES* dropFiles = static_cast<DROPFILES*>(GlobalLock(memory));
    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = TRUE;

    wchar_t* cursor = reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(dropFiles) + sizeof(DROPFILES));
    for (size_t index = 0; index < files.size(); ++index) {
        const std::wstring& path = files[index];
        memcpy(cursor, path.c_str(), (path.size() + 1) * sizeof(wchar_t));
        cursor += path.size() + 1;
    }
    *cursor = L'\0';

    GlobalUnlock(memory);
    SetClipboardData(CF_HDROP, memory);
    CloseClipboard();

    return true;
}

bool ClipboardMonitor::putTextOnClipboard(const std::wstring& text) const {
    if (text.empty() || !OpenClipboard(messageWindow_)) {
        return false;
    }

    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (memory == nullptr) {
        CloseClipboard();
        return false;
    }

    wchar_t* buffer = static_cast<wchar_t*>(GlobalLock(memory));
    memcpy(buffer, text.c_str(), bytes);
    GlobalUnlock(memory);
    SetClipboardData(CF_UNICODETEXT, memory);
    CloseClipboard();
    return true;
}
