#include "csv_reader.hpp"

#include <stdexcept>
#include <vector>

namespace bpt {

CsvReader::CsvReader(const std::string& path) : input_(path) {
    if (!input_) {
        throw std::runtime_error("Could not open CSV file: " + path);
    }
}

bool CsvReader::parse_two_columns(const std::string& raw, std::string& a, std::string& b) {
    std::string line = raw;
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    // Fast path for the provided dataset: simple key,payload with no quoted commas.
    if (line.empty() || line.front() != '"') {
        const auto pos = line.find(',');
        if (pos == std::string::npos) return false;
        a = line.substr(0, pos);
        b = line.substr(pos + 1);
        return true;
    }

    // Small RFC-4180 style parser, enough for two CSV fields and escaped quotes.
    std::vector<std::string> fields;
    std::string current;
    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == ',' && !in_quotes) {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    fields.push_back(current);

    if (fields.size() < 2) return false;
    a = std::move(fields[0]);
    b = std::move(fields[1]);
    // If there are extra columns, preserve them as part of payload instead of silently dropping data.
    for (std::size_t i = 2; i < fields.size(); ++i) {
        b.push_back(',');
        b += fields[i];
    }
    return true;
}

bool CsvReader::next(Record& out) {
    std::string line;
    while (std::getline(input_, line)) {
        ++line_number_;
        if (line.empty()) continue;

        std::string key, payload;
        if (!parse_two_columns(line, key, payload)) {
            throw std::runtime_error("Malformed CSV at line " + std::to_string(line_number_));
        }

        if (first_row_) {
            first_row_ = false;
            if (key == "key" && payload == "payload") {
                continue;
            }
        }

        out.key = std::move(key);
        out.payload = std::move(payload);
        return true;
    }
    return false;
}

} // namespace bpt
