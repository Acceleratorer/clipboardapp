#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <string>
#include <vector>

enum MultitextSeparatorMode {
    MultitextSeparatorSpace = 0,
    MultitextSeparatorBlankLine = 1,
    MultitextSeparatorNewline = 2,
    MultitextSeparatorCustom = 3
};

struct AppConfig {
    int textThreshold;
    int maxItems;
    int maxFileSizeMB;
    int cacheMaxAgeDays;
    bool askBeforeConverting;
    bool enableFileDialogAutoDetect;
    bool startWithWindows;
    int multitextSeparatorMode;
    std::wstring multitextCustomSeparator;
    std::vector<std::wstring> allowedProcesses;

    AppConfig()
        : textThreshold(1800), maxItems(30), maxFileSizeMB(100), cacheMaxAgeDays(7),
          askBeforeConverting(true), enableFileDialogAutoDetect(true), startWithWindows(false),
          multitextSeparatorMode(MultitextSeparatorBlankLine), multitextCustomSeparator(L"\\n") {}
};

class AppState {
public:
    bool initialize();
    void load();
    void save() const;

    const AppConfig& config() const { return config_; }
    AppConfig& config() { return config_; }

    const std::wstring& dataDir() const { return dataDir_; }
    const std::wstring& cacheDir() const { return cacheDir_; }
    const std::wstring& configPath() const { return configPath_; }
    const std::wstring& libraryPath() const { return libraryPath_; }

private:
    AppConfig config_;
    std::wstring dataDir_;
    std::wstring cacheDir_;
    std::wstring configPath_;
    std::wstring libraryPath_;
};

std::wstring numberToWString(size_t value);
std::wstring trim(std::wstring value);
std::wstring toLower(std::wstring value);
std::string wideToUtf8(const std::wstring& value);
std::wstring utf8ToWide(const std::string& value);
std::wstring pathJoin(const std::wstring& left, const std::wstring& right);
std::wstring fileNameFromPath(const std::wstring& path);
std::wstring extensionFromPath(const std::wstring& path);
std::wstring fileStemFromPath(const std::wstring& path);
std::wstring safeFilePart(std::wstring value);
std::wstring uniquePath(const std::wstring& directory, const std::wstring& prefix, const std::wstring& extension);
bool fileExists(const std::wstring& path);
bool directoryExists(const std::wstring& path);
bool ensureDirectory(const std::wstring& path);
bool copyFileToPath(const std::wstring& source, const std::wstring& destination);
bool deleteFileIfExists(const std::wstring& path);
bool clearDirectoryContents(const std::wstring& directory);
bool clearDirectoryContentsExcept(const std::wstring& directory, const std::vector<std::wstring>& preservedFiles);
bool cleanOldCacheFiles(const std::wstring& directory, int maxAgeDays);
bool setStartWithWindows(bool enabled);
bool writeBinaryFile(const std::wstring& path, const void* data, size_t size);
bool writeUtf8File(const std::wstring& path, const std::wstring& content, bool withBom);
std::wstring readUtf8File(const std::wstring& path);
