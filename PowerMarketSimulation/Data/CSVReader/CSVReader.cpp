#include "Data/CSVReader/CSVReader.h"

#include <filesystem>
#include <fstream>

namespace pms {

bool CSVReader::read(const std::string& path,
                     std::vector<Row>& rows,
                     std::string& errorMessage) {
    // 用 std::filesystem::u8path 把 UTF-8 路径转换为平台原生宽路径，
    // 避免中文目录在 Windows(GBK 代码页) 下用窄字符路径打不开文件。
    std::ifstream file(std::filesystem::u8path(path));
    if (!file.is_open()) {
        errorMessage = "无法打开文件: " + path;
        return false;
    }

    rows.clear();
    std::string line;
    bool isFirstLine = true;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (isFirstLine) {
            isFirstLine = false;
            // 去掉 UTF-8 BOM，兼容 Excel 导出的 CSV。
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
        }
        // 跳过空白行，文件末尾多余的空行不能再被当成一行数据。
        if (line.empty()) {
            continue;
        }
        rows.push_back(splitLine(line));
    }
    return true;
}

CSVReader::Row CSVReader::splitLine(const std::string& line) {
    Row cells;
    std::string cell;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (inQuotes) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cell += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cell += ch;
            }
        } else {
            if (ch == '"') {
                inQuotes = true;
            } else if (ch == ',') {
                cells.push_back(cell);
                cell.clear();
            } else {
                cell += ch;
            }
        }
    }

    cells.push_back(cell);
    return cells;
}

} // namespace pms
