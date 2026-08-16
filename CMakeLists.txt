cmake_minimum_required(VERSION 3.20)
project(ClipboardTxtApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ClipboardTxtApp WIN32
    src/main.cpp
    src/AppState.cpp
    src/AllowedApps.cpp
    src/ClipboardLibrary.cpp
    src/ClipboardMonitor.cpp
    src/FileDialogAssist.cpp
    src/TextFileConverter.cpp
    src/Ui.cpp
)

target_include_directories(ClipboardTxtApp PRIVATE src)
target_compile_definitions(ClipboardTxtApp PRIVATE UNICODE _UNICODE NOMINMAX _WIN32_WINNT=0x0501)
target_link_libraries(ClipboardTxtApp PRIVATE shell32 user32 gdi32 ole32 uuid psapi gdiplus advapi32)

if (MSVC)
    target_compile_options(ClipboardTxtApp PRIVATE /W4 /permissive-)
endif()
