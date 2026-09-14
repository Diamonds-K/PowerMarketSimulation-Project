#pragma once

#include <string>
#include <vector>

namespace pms {

class CSVReader {
public:
    using Row = std::vector<std::string>;

    static bool read(const std::string& path, std::vector<Row>& rows, std::string& errorMessage);
    static Row splitLine(const std::string& line);
};

} // namespace pms
