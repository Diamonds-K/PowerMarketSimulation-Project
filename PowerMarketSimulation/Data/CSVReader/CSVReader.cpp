#include "Data/CSVReader/CSVReader.h"

#include <fstream>

namespace pms {

bool CSVReader::read(const std::string& path,
                     std::vector<Row>& rows,
                     std::string& errorMessage) {
    std::ifstream file(path);
    if (!file.is_open()) {
        errorMessage = "无法打开文件: " + path;
        return false;
    }

    rows.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
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
