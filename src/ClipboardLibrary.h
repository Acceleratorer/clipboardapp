#pragma once

#include "AppState.h"

#include <string>
#include <vector>

struct ClipboardItem {
    enum Type {
        Text,
        Files,
        Image
    };

    Type type;
    std::wstring title;
    std::wstring text;
    std::vector<std::wstring> files;
    std::vector<std::wstring> originalFiles;
    std::wstring createdAt;
    std::wstring previewPath;
    bool cached;
    bool pinned;

    ClipboardItem() : type(Text), cached(false), pinned(false) {}
};

class ClipboardLibrary {
public:
    ClipboardLibrary(std::wstring storagePath, const AppConfig& config);

    void load();
    void save() const;
    void addText(const std::wstring& text);
    void addFiles(const std::vector<std::wstring>& cachedFiles, const std::vector<std::wstring>& originalFiles);
    void addImage(const std::wstring& cachedImagePath, const std::wstring& title);

    const std::vector<ClipboardItem>& items() const { return items_; }
    ClipboardItem* itemAt(size_t index);
    int togglePinned(size_t index);
    void trimToLimit();
    std::vector<std::wstring> pinnedCachePaths() const;
    void removeCachedItems();

private:
    size_t unpinnedInsertIndex() const;
    void removeOwnedCache(const ClipboardItem& item) const;

    std::wstring storagePath_;
    const AppConfig& config_;
    std::vector<ClipboardItem> items_;
};
