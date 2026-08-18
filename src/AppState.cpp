#include "AppState.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
std::wstring roamingAppDataPath() {
    wchar_t buffer[MAX_PATH]{};
    GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH);
    return buffer;
}

std::vector<std::wstring> defaultAllowedProcesses() {
    std::vector<std::wstring> values;
    values.push_back(L"chrome.exe");
    values.push_back(L"msedge.exe");
    values.push_back(L"firefox.exe");
    values.push_back(L"brave.exe");
    values.push_back(L"opera.exe");
    values.push_back(L"discord.exe");
    values.push_back(L"notepad.exe");
    return values;
}

std::wstring joinProcesses(const std::vector<std::wstring>& values) {
    std::wstring result;
    for (size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            result += L",";
        }
        result += values[index];
    }
    return result;
}

std::vector<std::wstring> splitProcesses(const std::wstring& value) {
    std::vector<std::wstring> result;
    std::wstringstream stream(value);
    std::wstring part;
    while (std::getline(stream, part, L',')) {
        part = trim(part);
        if (!part.empty()) {
            result.push_back(toLower(part));
        }
    }
    return result;
}

bool parseBool(const std::wstring& value, bool fallback) {
    const std::wstring lowered = toLower(trim(value));
    if (lowered == L"true" || lowered == L"1" || lowered == L"yes") {
        return true;
    }
    if (lowered == L"false" || lowered == L"0" || lowered == L"no") {
        return false;
    }
    return fallback;
}

std::vector<std::wstring> splitLines(const std::wstring& value) {
    std::vector<std::wstring> lines;
    std::wstringstream stream(value);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.size() - 1] == L'\r') {
            line.resize(line.size() - 1);
        }
        lines.push_back(line);
    }
    return lines;
}

std::wstring currentExecutablePath() {
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, &buffer[0], static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return L"";
    }
    return std::wstring(&buffer[0], length);
}
} // namespace

bool AppState::initialize() {
    dataDir_ = pathJoin(roamingAppDataPath(), L"ClipboardTxtApp");
    cacheDir_ = pathJoin(dataDir_, L"generated");
    configPath_ = pathJoin(dataDir_, L"config.ini");
    libraryPath_ = pathJoin(dataDir_, L"library.tsv");

    if (!ensureDirectory(cacheDir_)) {
        return false;
    }

    config_.allowedProcesses = defaultAllowedProcesses();
    load();
    setStartWithWindows(config_.startWithWindows);
    if (config_.cacheMaxAgeDays > 0) {
        cleanOldCacheFiles(cacheDir_, config_.cacheMaxAgeDays);
    }
    save();
    return true;
}

void AppState::load() {
    const std::wstring content = readUtf8File(configPath_);
    if (content.empty()) {
        return;
    }

    const std::vector<std::wstring> lines = splitLines(content);
    for (size_t index = 0; index < lines.size(); ++index) {
        const std::wstring& line = lines[index];
        const size_t separator = line.find(L'=');
        if (separator == std::wstring::npos) {
            continue;
        }

        const std::wstring key = trim(line.substr(0, separator));
        const std::wstring value = trim(line.substr(separator + 1));
        if (key == L"textThreshold") {
            config_.textThreshold = std::max(0, _wtoi(value.c_str()));
        } else if (key == L"maxItems") {
            int parsed = _wtoi(value.c_str());
            if (parsed < 0) {
                parsed = 0;
            }
            if (parsed > 500) {
                parsed = 500;
            }
            config_.maxItems = parsed;
        } else if (key == L"maxFileSizeMB") {
            int parsed = _wtoi(value.c_str());
            if (parsed < 0) {
                parsed = 0;
            }
            if (parsed > 10240) {
                parsed = 10240;
            }
            config_.maxFileSizeMB = parsed;
        } else if (key == L"cacheMaxAgeDays") {
            int parsed = _wtoi(value.c_str());
            if (parsed < 0) {
                parsed = 0;
            }
            if (parsed > 36500) {
                parsed = 36500;
            }
            config_.cacheMaxAgeDays = parsed;
        } else if (key == L"askBeforeConverting") {
            config_.askBeforeConverting = parseBool(value, config_.askBeforeConverting);
        } else if (key == L"enableFileDialogAutoDetect") {
            config_.enableFileDialogAutoDetect = parseBool(value, config_.enableFileDialogAutoDetect);
        } else if (key == L"startWithWindows") {
            config_.startWithWindows = parseBool(value, config_.startWithWindows);
        } else if (key == L"multitextSeparatorMode") {
            int parsed = _wtoi(value.c_str());
            if (parsed < MultitextSeparatorSpace || parsed > MultitextSeparatorCustom) {
                parsed = MultitextSeparatorNewline;
            }
            config_.multitextSeparatorMode = parsed;
        } else if (key == L"multitextCustomSeparator") {
            config_.multitextCustomSeparator = value;
        } else if (key == L"allowedProcesses") {
            std::vector<std::wstring> processes = splitProcesses(value);
            if (!processes.empty()) {
                config_.allowedProcesses = processes;
            }
        }
    }
}

void AppState::save() const {
    std::wstringstream stream;
    stream << L"textThreshold=" << config_.textThreshold << L"\n";
    stream << L"maxItems=" << config_.maxItems << L"\n";
    stream << L"maxFileSizeMB=" << config_.maxFileSizeMB << L"\n";
    stream << L"cacheMaxAgeDays=" << config_.cacheMaxAgeDays << L"\n";
    stream << L"askBeforeConverting=" << (config_.askBeforeConverting ? L"true" : L"false") << L"\n";
    stream << L"enableFileDialogAutoDetect=" << (config_.enableFileDialogAutoDetect ? L"true" : L"false") << L"\n";
    stream << L"startWithWindows=" << (config_.startWithWindows ? L"true" : L"false") << L"\n";
    stream << L"multitextSeparatorMode=" << config_.multitextSeparatorMode << L"\n";
    stream << L"multitextCustomSeparator=" << config_.multitextCustomSeparator << L"\n";
    stream << L"allowedProcesses=" << joinProcesses(config_.allowedProcesses) << L"\n";
    writeUtf8File(configPath_, stream.str(), true);
}

std::wstring numberToWString(size_t value) {
    std::wstringstream stream;
    stream << value;
    return stream.str();
}

std::wstring trim(std::wstring value) {
    const size_t first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return L"";
    }
    const size_t last = value.find_last_not_of(L" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::wstring toLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
}

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &result[0], size);
    return result;
}

std::wstring pathJoin(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (left[left.size() - 1] == L'\\' || left[left.size() - 1] == L'/') {
        return left + right;
    }
    return left + L"\\" + right;
}

std::wstring fileNameFromPath(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::wstring extensionFromPath(const std::wstring& path) {
    const std::wstring name = fileNameFromPath(path);
    const size_t dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos) {
        return L"";
    }
    return name.substr(dot);
}

std::wstring fileStemFromPath(const std::wstring& path) {
    const std::wstring name = fileNameFromPath(path);
    const size_t dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos || dot == 0) {
        return name;
    }
    return name.substr(0, dot);
}

std::wstring safeFilePart(std::wstring value) {
    value = trim(value);
    if (value.empty()) {
        value = L"clipboard";
    }
    for (size_t index = 0; index < value.size(); ++index) {
        wchar_t& character = value[index];
        if (character == L'<' || character == L'>' || character == L':' || character == L'"' ||
            character == L'/' || character == L'\\' || character == L'|' || character == L'?' || character == L'*' ||
            character == L'\r' || character == L'\n' || character == L'\t') {
            character = L'_';
        }
    }
    if (value.size() > 80) {
        value.resize(80);
    }
    return value;
}

std::wstring uniquePath(const std::wstring& directory, const std::wstring& prefix, const std::wstring& extension) {
    ensureDirectory(directory);
    SYSTEMTIME time;
    GetLocalTime(&time);

    std::wstringstream base;
    base << safeFilePart(prefix) << L"_"
         << time.wYear
         << (time.wMonth < 10 ? L"0" : L"") << time.wMonth
         << (time.wDay < 10 ? L"0" : L"") << time.wDay
         << L"_"
         << (time.wHour < 10 ? L"0" : L"") << time.wHour
         << (time.wMinute < 10 ? L"0" : L"") << time.wMinute
         << (time.wSecond < 10 ? L"0" : L"") << time.wSecond
         << L"_" << time.wMilliseconds;

    std::wstring normalizedExtension = extension;
    if (!normalizedExtension.empty() && normalizedExtension[0] != L'.') {
        normalizedExtension = L"." + normalizedExtension;
    }

    std::wstring path = pathJoin(directory, base.str() + normalizedExtension);
    for (size_t counter = 2; fileExists(path); ++counter) {
        path = pathJoin(directory, base.str() + L"_" + numberToWString(counter) + normalizedExtension);
    }
    return path;
}

bool fileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool directoryExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool ensureDirectory(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }

    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        ensureDirectory(path.substr(0, slash));
    }
    return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool copyFileToPath(const std::wstring& source, const std::wstring& destination) {
    const size_t slash = destination.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        ensureDirectory(destination.substr(0, slash));
    }
    return CopyFileW(source.c_str(), destination.c_str(), FALSE) != FALSE;
}

bool deleteFileIfExists(const std::wstring& path) {
    if (!fileExists(path)) {
        return true;
    }
    return DeleteFileW(path.c_str()) != FALSE;
}

bool clearDirectoryContents(const std::wstring& directory) {
    return clearDirectoryContentsExcept(directory, std::vector<std::wstring>());
}

bool clearDirectoryContentsExcept(const std::wstring& directory, const std::vector<std::wstring>& preservedFiles) {
    if (!directoryExists(directory)) {
        return false;
    }

    WIN32_FIND_DATAW data;
    const std::wstring pattern = pathJoin(directory, L"*");
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        const std::wstring name = data.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }

        const std::wstring path = pathJoin(directory, name);
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            clearDirectoryContentsExcept(path, preservedFiles);
            RemoveDirectoryW(path.c_str());
        } else {
            bool preserved = false;
            for (size_t index = 0; index < preservedFiles.size(); ++index) {
                if (_wcsicmp(path.c_str(), preservedFiles[index].c_str()) == 0) {
                    preserved = true;
                    break;
                }
            }
            if (!preserved) {
                DeleteFileW(path.c_str());
            }
        }
    } while (FindNextFileW(find, &data));

    FindClose(find);
    return true;
}

bool cleanOldCacheFiles(const std::wstring& directory, int maxAgeDays) {
    if (maxAgeDays <= 0 || !directoryExists(directory)) {
        return false;
    }

    FILETIME nowFileTime;
    GetSystemTimeAsFileTime(&nowFileTime);
    ULARGE_INTEGER now;
    now.LowPart = nowFileTime.dwLowDateTime;
    now.HighPart = nowFileTime.dwHighDateTime;
    const unsigned long long maxAgeTicks = static_cast<unsigned long long>(maxAgeDays) * 24ULL * 60ULL * 60ULL * 10000000ULL;

    WIN32_FIND_DATAW data;
    const std::wstring pattern = pathJoin(directory, L"*");
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        const std::wstring name = data.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }

        const std::wstring path = pathJoin(directory, name);
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            cleanOldCacheFiles(path, maxAgeDays);
            RemoveDirectoryW(path.c_str());
            continue;
        }

        ULARGE_INTEGER modified;
        modified.LowPart = data.ftLastWriteTime.dwLowDateTime;
        modified.HighPart = data.ftLastWriteTime.dwHighDateTime;
        if (now.QuadPart > modified.QuadPart && now.QuadPart - modified.QuadPart > maxAgeTicks) {
            DeleteFileW(path.c_str());
        }
    } while (FindNextFileW(find, &data));

    FindClose(find);
    return true;
}

bool setStartWithWindows(bool enabled) {
    const wchar_t* keyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* valueName = L"ClipboardTxtApp";
    HKEY key = nullptr;

    if (!enabled) {
        const LONG openResult = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_SET_VALUE, &key);
        if (openResult == ERROR_FILE_NOT_FOUND) {
            return true;
        }
        if (openResult != ERROR_SUCCESS) {
            return false;
        }
        const LONG deleteResult = RegDeleteValueW(key, valueName);
        RegCloseKey(key);
        return deleteResult == ERROR_SUCCESS || deleteResult == ERROR_FILE_NOT_FOUND;
    }

    const std::wstring executablePath = currentExecutablePath();
    if (executablePath.empty()) {
        return false;
    }

    const LONG createResult = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        keyPath,
        0,
        nullptr,
        0,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);
    if (createResult != ERROR_SUCCESS) {
        return false;
    }

    const std::wstring command = L"\"" + executablePath + L"\"";
    const LONG setResult = RegSetValueExW(
        key,
        valueName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()),
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return setResult == ERROR_SUCCESS;
}

bool writeBinaryFile(const std::wstring& path, const void* data, size_t size) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        ensureDirectory(path.substr(0, slash));
    }

    FILE* file = _wfopen(path.c_str(), L"wb");
    if (file == nullptr) {
        return false;
    }
    if (data != nullptr && size > 0) {
        fwrite(data, 1, size, file);
    }
    fclose(file);
    return true;
}

bool writeUtf8File(const std::wstring& path, const std::wstring& content, bool withBom) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        ensureDirectory(path.substr(0, slash));
    }

    FILE* file = _wfopen(path.c_str(), L"wb");
    if (file == nullptr) {
        return false;
    }

    if (withBom) {
        const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        fwrite(bom, 1, sizeof(bom), file);
    }
    const std::string utf8 = wideToUtf8(content);
    if (!utf8.empty()) {
        fwrite(utf8.data(), 1, utf8.size(), file);
    }
    fclose(file);
    return true;
}

std::wstring readUtf8File(const std::wstring& path) {
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
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF && static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }
    return utf8ToWide(bytes);
}
