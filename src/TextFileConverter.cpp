#include "TextFileConverter.h"

#include "AppState.h"

#include <Windows.h>

#include <ctime>
#include <iomanip>
#include <sstream>

namespace {
std::wstring safePrefix(std::wstring prefix) {
    if (prefix.empty()) {
        prefix = L"clipboard";
    }

    for (size_t index = 0; index < prefix.size(); ++index) {
        wchar_t& character = prefix[index];
        if (character == L'<' || character == L'>' || character == L':' || character == L'"' ||
            character == L'/' || character == L'\\' || character == L'|' || character == L'?' || character == L'*') {
            character = L'_';
        }
    }
    return prefix;
}

std::wstring timestampName() {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    std::wstringstream stream;
    if (localTime != nullptr) {
        stream << std::put_time(localTime, L"%Y%m%d_%H%M%S");
    }
    return stream.str();
}

std::wstring ensureTxtExtension(std::wstring fileName) {
    fileName = safeFilePart(fileName);
    if (fileName.empty()) {
        fileName = L"clipboard";
    }
    if (extensionFromPath(fileName).empty()) {
        fileName += L".txt";
    }
    return fileName;
}

std::wstring uniqueExactPath(const std::wstring& directory, const std::wstring& fileName) {
    ensureDirectory(directory);
    const std::wstring safeName = ensureTxtExtension(fileName);
    const std::wstring stem = fileStemFromPath(safeName);
    const std::wstring extension = extensionFromPath(safeName).empty() ? L".txt" : extensionFromPath(safeName);
    std::wstring path = pathJoin(directory, safeName);
    for (size_t counter = 2; fileExists(path); ++counter) {
        path = pathJoin(directory, stem + L"_" + numberToWString(counter) + extension);
    }
    return path;
}
} // namespace

TextFileConverter::TextFileConverter(std::wstring outputDir) : outputDir_(outputDir) {}

std::wstring TextFileConverter::createTextFile(const std::wstring& text, const std::wstring& prefix) const {
    ensureDirectory(outputDir_);

    std::wstring path = pathJoin(outputDir_, safePrefix(prefix) + L"_" + timestampName() + L".txt");
    for (int counter = 2; fileExists(path); ++counter) {
        std::wstringstream name;
        name << safePrefix(prefix) << L"_" << timestampName() << L"_" << counter << L".txt";
        path = pathJoin(outputDir_, name.str());
    }

    writeUtf8File(path, text, true);
    return path;
}

std::wstring TextFileConverter::createTextFileWithName(const std::wstring& text, const std::wstring& fileName) const {
    const std::wstring path = uniqueExactPath(outputDir_, fileName);
    writeUtf8File(path, text, true);
    return path;
}
