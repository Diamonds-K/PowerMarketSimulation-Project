#pragma once

#include <string>
#include <vector>

namespace pms {

class CSVWriter {
public:
    using Row = std::vector<std::string>;

    static bool write(const std::string& path,
                      const std::vector<Row>& rows,
                      std::string& errorMessage);
    static std::string escape(const std::string& cell);
};

} // namespace pms
