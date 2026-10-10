#pragma once

#include "common.hpp"

#include <fstream>
#include <string>

namespace bpt {

class CsvReader {
public:
    explicit CsvReader(const std::string& path);
    bool next(Record& out);
    std::uint64_t line_number() const { return line_number_; }

private:
    std::ifstream input_;
    bool first_row_ = true;
    std::uint64_t line_number_ = 0;

    static bool parse_two_columns(const std::string& line, std::string& a, std::string& b);
};

} // namespace bpt
