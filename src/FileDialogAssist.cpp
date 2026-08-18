#include "FileDialogAssist.h"

#include "ClipboardMonitor.h"
#include "Ui.h"

#include <cwchar>
#include <sstream>
#include <vector>

namespace {
bool classNameEquals(HWND window, const wchar_t* expected) {
    wchar_t className[128];
    ZeroMemory(className, sizeof(className));
    GetClassNameW(window, className, 128);
    return wcscmp(className, expected) == 0;
}

BOOL CALLBACK findComboBoxExProc(HWND child, LPARAM param) {
    if (classNameEquals(child, L"ComboBoxEx32")) {
        *reinterpret_cast<HWND*>(param) = child;
        return FALSE;
    }
    return TRUE;
}

BOOL CALLBACK findEditProc(HWND child, LPARAM param) {
    if (classNameEquals(child, L"Edit")) {
        *reinterpret_cast<HWND*>(param) = child;
        return FALSE;
    }
    EnumChildWindows(child, findEditProc, param);
    return *reinterpret_cast<HWND*>(param) == nullptr;
}

BOOL CALLBACK enumTopLevelDialogs(HWND window, LPARAM param) {
    if (!IsWindowVisible(window)) {
        return TRUE;
    }
    if (classNameEquals(window, L"#32770")) {
        std::vector<HWND>* dialogs = reinterpret_cast<std::vector<HWND>*>(param);
        dialogs->push_back(window);
    }
    return TRUE;
}

std::wstring uniqueSiblingPath(const std::wstring& sourcePath, const std::wstring& requestedName) {
    const size_t slash = sourcePath.find_last_of(L"\\/");
    const std::wstring directory = slash == std::wstring::npos ? L"" : sourcePath.substr(0, slash);
    std::wstring safeName = safeFilePart(requestedName);
    if (safeName.empty()) {
        safeName = fileNameFromPath(sourcePath);
    }
    if (extensionFromPath(safeName).empty()) {
        safeName += extensionFromPath(sourcePath);
    }

    const std::wstring stem = fileStemFromPath(safeName);
    const std::wstring extension = extensionFromPath(safeName);
    std::wstring path = pathJoin(directory, safeName);
    for (size_t counter = 2; fileExists(path); ++counter) {
        path = pathJoin(directory, stem + L"_" + numberToWString(counter) + extension);
    }
    return path;
}

void sendEnter(HWND window) {
    SetForegroundWindow(window);
    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_RETURN;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_RETURN;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

std::wstring readTextFileForPaste(const std::wstring& path) {
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (file == nullptr) {
        return L"";
    }
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        fclose(file);
        return L"";
    }
    std::string bytes(static_cast<size_t>(size), '\0');
    fread(&bytes[0], 1, bytes.size(), file);
    fclose(file);
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) {
        std::wstring result;
        for (size_t index = 2; index + 1 < bytes.size(); index += 2) {
            result += static_cast<wchar_t>(static_cast<unsigned char>(bytes[index]) | (static_cast<unsigned char>(bytes[index + 1]) << 8));
        }
        return result;
    }
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF) {
        std::wstring result;
        for (size_t index = 2; index + 1 < bytes.size(); index += 2) {
            result += static_cast<wchar_t>((static_cast<unsigned char>(bytes[index]) << 8) | static_cast<unsigned char>(bytes[index + 1]));
        }
        return result;
    }
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF && static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }
    const std::wstring utf8 = utf8ToWide(bytes);
    if (!utf8.empty() || bytes.empty()) {
        return utf8;
    }
    const int wideSize = MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (wideSize <= 0) {
        return L"";
    }
    std::wstring ansi(static_cast<size_t>(wideSize), L'\0');
    MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), &ansi[0], wideSize);
    return ansi;
}

} // namespace

FileDialogAssist::FileDialogAssist(const AppConfig& config, const AllowedApps& allowedApps, ClipboardLibrary& library, TextFileConverter& converter, ClipboardMonitor& clipboard, Ui& ui)
    : config_(config), allowedApps_(allowedApps), library_(library), converter_(converter), clipboard_(clipboard), ui_(ui), lastPromptedDialog_(nullptr) {}

void FileDialogAssist::handleHotkey() {
    assistActiveDialog(false);
}

void FileDialogAssist::checkAutoDetect() {
    if (!config_.enableFileDialogAutoDetect) {
        return;
    }

    HWND dialog = findOpenDialog();
    if (dialog == nullptr || dialog == lastPromptedDialog_) {
        return;
    }
    if (!allowedApps_.isWindowAllowed(GetWindow(dialog, GW_OWNER)) && !allowedApps_.isForegroundAllowed()) {
        return;
    }

    lastPromptedDialog_ = dialog;
    assistActiveDialog(true);
}

void FileDialogAssist::assistActiveDialog(bool sendTextAsFileByDefault) {
    HWND originalTarget = GetForegroundWindow();
    HWND originalDialog = findOpenDialog();

    ClipboardPickResult pick = ui_.chooseClipboardItem(sendTextAsFileByDefault);
    if (pick.selectedIndex < 0) {
        return;
    }

    ClipboardItem* item = library_.itemAt(static_cast<size_t>(pick.selectedIndex));
    if (item == nullptr) {
        return;
    }
    ClipboardItem multiTextItem;
    if (pick.multiText) {
        multiTextItem.type = ClipboardItem::Text;
        multiTextItem.title = L"Multitext";
        multiTextItem.text = pick.selectedText;
        item = &multiTextItem;
    }

    HWND dialog = IsWindow(originalDialog) ? originalDialog : findOpenDialog();
    if (dialog == nullptr) {
        pasteIntoActiveInput(pick, *item, originalTarget);
        return;
    }

    const std::wstring path = pathForItem(*item, pick.selectedFileName);
    if (path.empty()) {
        return;
    }

    if (fillDialogWithPath(dialog, path)) {
        return;
    }

    std::vector<std::wstring> files;
    files.push_back(path);
    clipboard_.putFilesOnClipboard(files);
    ui_.showInfo(L"Đã đặt file vào clipboard. Nếu hộp Choose file chưa tự điền, hãy paste đường dẫn hoặc kéo file này vào:\n" + path);
}


void FileDialogAssist::pasteIntoActiveInput(const ClipboardPickResult& pick, const ClipboardItem& item, HWND targetWindow) {
    HWND target = targetWindow;
    if (target == nullptr || !IsWindow(target)) {
        target = GetForegroundWindow();
    }
    if (target == nullptr) {
        return;
    }

    if (pick.extractTextFromFile && item.type == ClipboardItem::Files && item.files.size() == 1) {
        const std::wstring text = readTextFileForPaste(item.files.front());
        if (!text.empty() && clipboard_.putTextOnClipboard(text)) {
            clipboard_.sendPasteToWindow(target);
        }
        return;
    }

    if (item.type == ClipboardItem::Text && !pick.sendTextAsFile) {
        if (clipboard_.putTextOnClipboard(item.text)) {
            clipboard_.sendPasteToWindow(target);
        }
        return;
    }

    const std::wstring path = pathForItem(item, pick.selectedFileName);
    if (path.empty()) {
        return;
    }

    std::vector<std::wstring> files;
    files.push_back(path);
    if (clipboard_.putFilesOnClipboard(files)) {
        clipboard_.sendPasteToWindow(target);
    }
}

HWND FileDialogAssist::findOpenDialog() const {
    std::vector<HWND> dialogs;
    EnumWindows(enumTopLevelDialogs, reinterpret_cast<LPARAM>(&dialogs));

    HWND foreground = GetForegroundWindow();
    for (size_t index = 0; index < dialogs.size(); ++index) {
        HWND dialog = dialogs[index];
        if (dialog == foreground || IsChild(dialog, foreground) || GetWindow(dialog, GW_OWNER) == foreground) {
            return dialog;
        }
    }
    return nullptr;
}

bool FileDialogAssist::fillDialogWithPath(HWND dialog, const std::wstring& path) const {
    HWND edit = nullptr;
    EnumChildWindows(dialog, findComboBoxExProc, reinterpret_cast<LPARAM>(&edit));
    if (edit != nullptr) {
        HWND innerEdit = FindWindowExW(edit, nullptr, L"Edit", nullptr);
        if (innerEdit != nullptr) {
            edit = innerEdit;
        }
    }
    if (edit == nullptr) {
        EnumChildWindows(dialog, findEditProc, reinterpret_cast<LPARAM>(&edit));
    }
    if (edit == nullptr) {
        return false;
    }

    SetForegroundWindow(dialog);
    SendMessageW(edit, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(path.c_str()));
    sendEnter(dialog);
    return true;
}

std::wstring FileDialogAssist::pathForItem(const ClipboardItem& item, const std::wstring& selectedFileName) {
    if (item.type == ClipboardItem::Text) {
        const std::wstring fileName = selectedFileName.empty() ? L"clipboard_text.txt" : selectedFileName;
        return converter_.createTextFileWithName(item.text, fileName);
    }

    for (size_t index = 0; index < item.files.size(); ++index) {
        if (!fileExists(item.files[index])) {
            continue;
        }
        const std::wstring requestedName = selectedFileName.empty() ? fileNameFromPath(item.files[index]) : selectedFileName;
        if (safeFilePart(requestedName) == fileNameFromPath(item.files[index])) {
            return item.files[index];
        }
        const std::wstring renamedPath = uniqueSiblingPath(item.files[index], requestedName);
        if (copyFileToPath(item.files[index], renamedPath)) {
            return renamedPath;
        }
        return item.files[index];
    }

    ui_.showInfo(L"Không tìm thấy file cache đã lưu trong clipboard library.");
    return L"";
}
