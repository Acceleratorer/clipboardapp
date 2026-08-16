#include "ClipboardLibrary.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {
std::wstring nowString() {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    std::wstringstream stream;
    if (localTime != nullptr) {
        stream << std::put_time(localTime, L"%Y-%m-%d %H:%M:%S");
    }
    return stream.str();
}

std::wstring previewText(const std::wstring& text) {
    std::wstring preview = text.substr(0, 80);
    for (size_t index = 0; index < preview.size(); ++index) {
        wchar_t& character = preview[index];
        if (character == L'\r' || character == L'\n' || character == L'\t') {
            character = L' ';
        }
    }
    if (text.size() > preview.size()) {
        preview += L"...";
    }
    return preview;
}

std::wstring encodeField(const std::wstring& value) {
    std::wstring encoded;
    for (size_t index = 0; index < value.size(); ++index) {
        const wchar_t character = value[index];
        if (character == L'\\') {
            encoded += L"\\\\";
        } else if (character == L'\t') {
            encoded += L"\\t";
        } else if (character == L'\r') {
            encoded += L"\\r";
        } else if (character == L'\n') {
            encoded += L"\\n";
        } else if (character == L'|') {
            encoded += L"\\p";
        } else {
            encoded += character;
        }
    }
    return encoded;
}

std::wstring decodeField(const std::wstring& value) {
    std::wstring decoded;
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] != L'\\' || index + 1 >= value.size()) {
            decoded += value[index];
            continue;
        }

        const wchar_t next = value[++index];
        if (next == L'\\') {
            decoded += L'\\';
        } else if (next == L't') {
            decoded += L'\t';
        } else if (next == L'r') {
            decoded += L'\r';
        } else if (next == L'n') {
            decoded += L'\n';
        } else if (next == L'p') {
            decoded += L'|';
        } else {
            decoded += next;
        }
    }
    return decoded;
}

std::vector<std::wstring> splitTab(const std::wstring& line) {
    std::vector<std::wstring> result;
    std::wstringstream stream(line);
    std::wstring part;
    while (std::getline(stream, part, L'\t')) {
        result.push_back(part);
    }
    return result;
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

std::vector<std::wstring> splitFiles(const std::wstring& value) {
    std::vector<std::wstring> files;
    std::wstringstream stream(value);
    std::wstring part;
    while (std::getline(stream, part, L'|')) {
        if (!part.empty()) {
            files.push_back(decodeField(part));
        }
    }
    return files;
}

std::wstring joinFiles(const std::vector<std::wstring>& files) {
    std::wstring result;
    for (size_t index = 0; index < files.size(); ++index) {
        if (index > 0) {
            result += L"|";
        }
        result += encodeField(files[index]);
    }
    return result;
}

std::wstring typeToString(ClipboardItem::Type type) {
    if (type == ClipboardItem::Files) {
        return L"files";
    }
    if (type == ClipboardItem::Image) {
        return L"image";
    }
    return L"text";
}

ClipboardItem::Type parseType(const std::wstring& value) {
    if (value == L"files") {
        return ClipboardItem::Files;
    }
    if (value == L"image") {
        return ClipboardItem::Image;
    }
    return ClipboardItem::Text;
}
} // namespace

ClipboardLibrary::ClipboardLibrary(std::wstring storagePath, const AppConfig& config)
    : storagePath_(storagePath), config_(config) {}

void ClipboardLibrary::load() {
    items_.clear();

    const std::wstring content = readUtf8File(storagePath_);
    if (content.empty()) {
        return;
    }

    const std::vector<std::wstring> lines = splitLines(content);
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const std::vector<std::wstring> fields = splitTab(lines[lineIndex]);
        if (fields.size() < 4) {
            continue;
        }

        ClipboardItem item;
        item.type = parseType(fields[0]);
        item.createdAt = decodeField(fields[1]);
        item.title = decodeField(fields[2]);
        if (item.type == ClipboardItem::Text) {
            item.text = decodeField(fields[3]);
        } else {
            item.files = splitFiles(fields[3]);
        }

        if (fields.size() >= 5) {
            item.originalFiles = splitFiles(fields[4]);
        }
        if (fields.size() >= 6) {
            item.cached = decodeField(fields[5]) == L"1" || decodeField(fields[5]) == L"true";
        }
        if (fields.size() >= 7) {
            item.previewPath = decodeField(fields[6]);
        }
        if (item.type == ClipboardItem::Image && item.previewPath.empty() && !item.files.empty()) {
            item.previewPath = item.files.front();
        }
        items_.push_back(item);
    }
    trimToLimit();
}

void ClipboardLibrary::save() const {
    std::wstringstream stream;
    for (size_t index = 0; index < items_.size(); ++index) {
        const ClipboardItem& item = items_[index];
        stream << typeToString(item.type) << L'\t'
               << encodeField(item.createdAt) << L'\t'
               << encodeField(item.title) << L'\t';
        if (item.type == ClipboardItem::Text) {
            stream << encodeField(item.text);
        } else {
            stream << joinFiles(item.files);
        }
        stream << L'\t' << joinFiles(item.originalFiles)
               << L'\t' << (item.cached ? L"1" : L"0")
               << L'\t' << encodeField(item.previewPath)
               << L'\n';
    }
    writeUtf8File(storagePath_, stream.str(), true);
}

void ClipboardLibrary::addText(const std::wstring& text) {
    if (text.empty()) {
        return;
    }

    ClipboardItem item;
    item.type = ClipboardItem::Text;
    item.text = text;
    item.createdAt = nowString();
    item.title = L"Text (" + numberToWString(text.size()) + L" chars): " + previewText(text);
    items_.insert(items_.begin(), item);
    trimToLimit();
    save();
}

void ClipboardLibrary::addFiles(const std::vector<std::wstring>& cachedFiles, const std::vector<std::wstring>& originalFiles) {
    if (cachedFiles.empty()) {
        return;
    }

    ClipboardItem item;
    item.type = ClipboardItem::Files;
    item.files = cachedFiles;
    item.originalFiles = originalFiles;
    item.cached = true;
    item.createdAt = nowString();
    item.title = cachedFiles.size() == 1
        ? L"File cache: " + fileNameFromPath(originalFiles.empty() ? cachedFiles.front() : originalFiles.front())
        : L"File cache: " + numberToWString(cachedFiles.size()) + L" items";
    items_.insert(items_.begin(), item);
    trimToLimit();
    save();
}

void ClipboardLibrary::addImage(const std::wstring& cachedImagePath, const std::wstring& title) {
    if (cachedImagePath.empty()) {
        return;
    }

    ClipboardItem item;
    item.type = ClipboardItem::Image;
    item.files.push_back(cachedImagePath);
    item.previewPath = cachedImagePath;
    item.cached = true;
    item.createdAt = nowString();
    item.title = title.empty() ? L"Image cache: " + fileNameFromPath(cachedImagePath) : title;
    items_.insert(items_.begin(), item);
    trimToLimit();
    save();
}

ClipboardItem* ClipboardLibrary::itemAt(size_t index) {
    if (index >= items_.size()) {
        return nullptr;
    }
    return &items_[index];
}

void ClipboardLibrary::trimToLimit() {
    if (config_.maxItems <= 0) {
        return;
    }

    const size_t maxItems = static_cast<size_t>(config_.maxItems);
    while (items_.size() > maxItems) {
        removeOwnedCache(items_.back());
        items_.pop_back();
    }
}

void ClipboardLibrary::removeCachedItems() {
    std::vector<ClipboardItem> kept;
    for (size_t index = 0; index < items_.size(); ++index) {
        if (items_[index].type == ClipboardItem::Text) {
            kept.push_back(items_[index]);
        }
    }
    items_.swap(kept);
    save();
}

void ClipboardLibrary::removeOwnedCache(const ClipboardItem& item) const {
    if (!item.cached) {
        return;
    }
    for (size_t index = 0; index < item.files.size(); ++index) {
        deleteFileIfExists(item.files[index]);
    }
    if (!item.previewPath.empty()) {
        deleteFileIfExists(item.previewPath);
    }
}
