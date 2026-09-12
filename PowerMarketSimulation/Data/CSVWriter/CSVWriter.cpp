#include "Data/CSVWriter/CSVWriter.h"

#include <filesystem>
#include <fstream>

namespace pms {

bool CSVWriter::write(const std::string& path,
                      const std::vector<Row>& rows,
                      std::string& errorMessage) {
    // 用 std::filesystem::u8path 把 UTF-8 路径转换为平台原生宽路径，
    // 避免中文目录在 Windows(GBK 代码页) 下用窄字符路径创建不了文件。
    std::ofstream file(std::filesystem::u8path(path));
    if (!file.is_open()) {
        errorMessage = "无法创建文件: " + path;
        return false;
    }

    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) {
                file << ',';
            }
            file << escape(row[i]);
        }
        file << '\n';
    }

    if (!file.good()) {
        errorMessage = "写入文件失败: " + path;
        return false;
    }
    return true;
}

std::string CSVWriter::escape(const std::string& cell) {
    const bool needsQuotes =
        cell.find(',') != std::string::npos ||
        cell.find('"') != std::string::npos ||
        cell.find('\n') != std::string::npos;
    if (!needsQuotes) {
        return cell;
    }

    std::string escaped = "\"";
    for (const char ch : cell) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped += ch;
        }
    }
    escaped += '"';
    return escaped;
}

} // namespace pms
