#include "AllowedApps.h"
#include "AppState.h"
#include "ClipboardLibrary.h"
#include "ClipboardMonitor.h"
#include "FileDialogAssist.h"
#include "TextFileConverter.h"
#include "Ui.h"

#include <Windows.h>

#include <memory>

namespace {
const UINT TrayMessage = WM_APP + 1;
const UINT PasteShortcutMessage = WM_APP + 2;
const UINT HotkeyFileAssist = 1;
const UINT AutoDetectTimer = 1;

struct Runtime {
    AppState state;
    std::unique_ptr<ClipboardLibrary> library;
    std::unique_ptr<TextFileConverter> converter;
    std::unique_ptr<AllowedApps> allowedApps;
    std::unique_ptr<Ui> ui;
    std::unique_ptr<ClipboardMonitor> clipboard;
    std::unique_ptr<FileDialogAssist> fileAssist;
    HHOOK keyboardHook;

    Runtime() : keyboardHook(nullptr) {}
};

Runtime* runtime = nullptr;

LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && runtime && runtime->clipboard && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* keyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (keyboard->vkCode == 'V' &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
            (GetAsyncKeyState(VK_MENU) & 0x8000) == 0) {
            if (runtime->clipboard->shouldSkipPasteShortcut()) {
                runtime->clipboard->consumePasteShortcutSkip();
                return CallNextHookEx(runtime ? runtime->keyboardHook : nullptr, code, wParam, lParam);
            }
            HWND foreground = GetForegroundWindow();
            if (foreground != nullptr && runtime->clipboard->shouldInterceptPasteShortcut(foreground)) {
                PostMessageW(runtime->clipboard->messageWindow(), PasteShortcutMessage, 0, reinterpret_cast<LPARAM>(foreground));
                return 1;
            }
        }
    }
    return CallNextHookEx(runtime ? runtime->keyboardHook : nullptr, code, wParam, lParam);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case PasteShortcutMessage:
        if (runtime && runtime->clipboard) {
            runtime->clipboard->prepareLongTextForPaste(reinterpret_cast<HWND>(lParam), true);
        }
        return 0;
    case WM_DRAWCLIPBOARD:
        if (runtime && runtime->clipboard) {
            runtime->clipboard->handleClipboardUpdate();
            HWND nextViewer = runtime->clipboard->nextClipboardViewer();
            if (nextViewer != nullptr) {
                SendMessageW(nextViewer, message, wParam, lParam);
            }
        }
        return 0;
    case WM_CHANGECBCHAIN:
        if (runtime && runtime->clipboard) {
            HWND nextViewer = runtime->clipboard->nextClipboardViewer();
            if (reinterpret_cast<HWND>(wParam) == nextViewer) {
                runtime->clipboard->stop();
                runtime->clipboard->start();
            } else if (nextViewer != nullptr) {
                SendMessageW(nextViewer, message, wParam, lParam);
            }
        }
        return 0;
    case WM_HOTKEY:
        if (wParam == HotkeyFileAssist && runtime && runtime->fileAssist) {
            runtime->fileAssist->handleHotkey();
        }
        return 0;
    case WM_TIMER:
        if (wParam == AutoDetectTimer) {
            if (runtime && runtime->fileAssist) {
                runtime->fileAssist->checkAutoDetect();
            }
        }
        return 0;
    case TrayMessage:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_LBUTTONUP) {
            if (runtime && runtime->ui) {
                runtime->ui->showTrayMenu();
            }
        }
        return 0;
    case WM_DESTROY:
        KillTimer(window, AutoDetectTimer);
        UnregisterHotKey(window, HotkeyFileAssist);
        if (runtime && runtime->clipboard) {
            runtime->clipboard->stop();
        }
        if (runtime && runtime->ui) {
            runtime->ui->removeTrayIcon();
        }
        if (runtime && runtime->keyboardHook != nullptr) {
            UnhookWindowsHookEx(runtime->keyboardHook);
            runtime->keyboardHook = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

HWND createMessageWindow(HINSTANCE instance) {
    const wchar_t* className = L"ClipboardTxtAppMessageWindow";
    WNDCLASSW windowClass;
    ZeroMemory(&windowClass, sizeof(windowClass));
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&windowClass);

    return CreateWindowExW(
        0,
        className,
        L"ClipboardTxtApp",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        320,
        200,
        nullptr,
        nullptr,
        instance,
        nullptr);
}

int runApp(HINSTANCE instance) {
    Runtime appRuntime;
    runtime = &appRuntime;

    if (!appRuntime.state.initialize()) {
        MessageBoxW(nullptr, L"Không thể khởi tạo thư mục dữ liệu trong APPDATA.", L"ClipboardTxtApp", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND window = createMessageWindow(instance);
    if (window == nullptr) {
        MessageBoxW(nullptr, L"Không thể tạo message window.", L"ClipboardTxtApp", MB_OK | MB_ICONERROR);
        return 1;
    }

    appRuntime.library.reset(new ClipboardLibrary(appRuntime.state.libraryPath(), appRuntime.state.config()));
    appRuntime.library->load();
    appRuntime.converter.reset(new TextFileConverter(appRuntime.state.cacheDir()));
    appRuntime.allowedApps.reset(new AllowedApps(appRuntime.state.config()));
    appRuntime.ui.reset(new Ui(instance, window, appRuntime.state, *appRuntime.library));
    appRuntime.clipboard.reset(new ClipboardMonitor(window, appRuntime.state.config(), *appRuntime.allowedApps, *appRuntime.library, *appRuntime.converter, *appRuntime.ui));
    appRuntime.fileAssist.reset(new FileDialogAssist(appRuntime.state.config(), *appRuntime.allowedApps, *appRuntime.library, *appRuntime.converter, *appRuntime.clipboard, *appRuntime.ui));

    appRuntime.ui->addTrayIcon();
    appRuntime.clipboard->start();
    appRuntime.keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, instance, 0);
    RegisterHotKey(window, HotkeyFileAssist, MOD_CONTROL | MOD_ALT, 'V');
    SetTimer(window, AutoDetectTimer, 1500, nullptr);

    MSG message;
    ZeroMemory(&message, sizeof(message));
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    runtime = nullptr;
    return 0;
}
} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    return runApp(instance);
}
