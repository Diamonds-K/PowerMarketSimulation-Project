#pragma once

#include <string>
#include <vector>

namespace pms {

class CSVReader {
public:
    using Row = std::vector<std::string>;

    // 读取 CSV 文件，成功时以二维字符串数组返回全部行。
    static bool read(const std::string& path, std::vector<Row>& rows, std::string& errorMessage);

    // 解析单行 CSV，并处理逗号、引号和转义字符。
    static Row splitLine(const std::string& line);
};

} // namespace pms
