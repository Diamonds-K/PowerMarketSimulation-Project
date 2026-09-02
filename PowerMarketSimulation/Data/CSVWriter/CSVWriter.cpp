#include "Data/CSVWriter/CSVWriter.h"

#include <fstream>

namespace pms {

bool CSVWriter::write(const std::string& path,
                      const std::vector<Row>& rows,
                      std::string& errorMessage) {
    std::ofstream file(path);
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
