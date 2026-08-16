#pragma once

#include <string>

class TextFileConverter {
public:
    explicit TextFileConverter(std::wstring outputDir);

    std::wstring createTextFile(const std::wstring& text, const std::wstring& prefix) const;
    std::wstring createTextFileWithName(const std::wstring& text, const std::wstring& fileName) const;

    const std::wstring& outputDir() const { return outputDir_; }

private:
    std::wstring outputDir_;
};
