#pragma once

#include <string>
#include <vector>

namespace pms {

class CSVWriter {
public:
    using Row = std::vector<std::string>;

    // 将二维字符串数组写入指定 CSV 文件。
    static bool write(const std::string& path,
                      const std::vector<Row>& rows,
                      std::string& errorMessage);

    // 对包含逗号、引号或换行的单元格进行 CSV 转义。
    static std::string escape(const std::string& cell);
};

} // namespace pms
