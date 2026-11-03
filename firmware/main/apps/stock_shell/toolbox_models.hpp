#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace stock_tools {
inline bool WindowExpired(uint64_t now, uint64_t start, uint64_t duration)
{
    return now < start || now - start >= duration;
}
// Original small implementations; no Flipper application code imported.
inline bool UrlTag(const std::string& url, const std::array<uint8_t, 7>& uid, std::array<uint8_t, 180>& mem)
{
    mem.fill(0);
    size_t prefix = url.rfind("https://", 0) == 0 ? 8 : url.rfind("http://", 0) == 0 ? 7 : 0;
    if (!prefix || url.size() <= prefix || url.size() > 96) return false;
    for (auto c : url)
        if (static_cast<unsigned char>(c) < 33 || static_cast<unsigned char>(c) > 126) return false;
    if (url.find_first_of("\\@") != std::string::npos) return false;
    for (int i = 0; i < 3; ++i) {
        mem[i] = uid[i];
    }
    mem[3] = 0x88 ^ uid[0] ^ uid[1] ^ uid[2];
    for (int i = 0; i < 4; ++i) {
        mem[4 + i] = uid[3 + i];
    }
    mem[8]  = uid[3] ^ uid[4] ^ uid[5] ^ uid[6];
    mem[12] = 0xe1;
    mem[13] = 0x10;
    mem[14] = 0x12;
    mem[15] = 0;
    auto n  = url.size() - prefix;
    mem[16] = 3;
    mem[17] = static_cast<uint8_t>(5 + n);
    mem[18] = 0xd1;
    mem[19] = 1;
    mem[20] = static_cast<uint8_t>(1 + n);
    mem[21] = 0x55;
    mem[22] = prefix == 8 ? 4 : 3;
    std::copy(url.begin() + prefix, url.end(), mem.begin() + 23);
    mem[23 + n] = 0xfe;
    return true;
}
inline bool Near(uint32_t value, uint32_t target)
{
    return value >= target * 3 / 4 && value <= target * 5 / 4;
}
inline bool NecDecode(const std::vector<uint32_t>& pulse, uint32_t& value)
{
    value = 0;
    if (pulse.size() != 67 || !Near(pulse[0], 9000) || !Near(pulse[1], 4500)) return false;
    for (int i = 0; i < 32; ++i) {
        if (!Near(pulse[2 + 2 * i], 560)) return false;
        if (Near(pulse[3 + 2 * i], 1690))
            value |= uint32_t(1) << i;
        else if (!Near(pulse[3 + 2 * i], 560))
            return false;
    }
    if (!Near(pulse[66], 560)) {
        value = 0;
        return false;
    }
    return true;
}
struct Game2048 {
    std::array<uint16_t, 16> cells{};
    uint32_t score = 0;
    void Reset()
    {
        cells.fill(0);
        score     = 0;
        cells[0]  = 2;
        cells[15] = 2;
    }
    bool Move(int direction)
    {
        if (direction < 0 || direction > 3) {
            return false;
        }
        auto before = cells;
        for (int line = 0; line < 4; ++line) {
            std::array<int, 4> index{};
            std::vector<uint16_t> values;
            for (int i = 0; i < 4; ++i) {
                index[i] = direction == 0   ? line * 4 + i
                           : direction == 1 ? line * 4 + 3 - i
                           : direction == 2 ? i * 4 + line
                                            : (3 - i) * 4 + line;
                if (cells[index[i]]) values.push_back(cells[index[i]]);
            }
            std::vector<uint16_t> merged;
            for (size_t i = 0; i < values.size(); ++i) {
                auto v = values[i];
                if (i + 1 < values.size() && values[i + 1] == v && v < 32768) {
                    v *= 2;
                    score = UINT32_MAX - score < v ? UINT32_MAX : score + v;
                    ++i;
                }
                merged.push_back(v);
            }
            for (int i = 0; i < 4; ++i) cells[index[i]] = i < static_cast<int>(merged.size()) ? merged[i] : 0;
        }
        return before != cells;
    }
    bool AddTile(uint32_t random)
    {
        std::vector<int> empty;
        for (int i = 0; i < 16; ++i)
            if (!cells[i]) empty.push_back(i);
        if (empty.empty()) return false;
        cells[empty[random % empty.size()]] = 2;
        return true;
    }
    bool CanMove() const
    {
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                auto v = cells[y * 4 + x];
                if (!v || (x < 3 && v == cells[y * 4 + x + 1] && v < 32768) ||
                    (y < 3 && v == cells[(y + 1) * 4 + x] && v < 32768))
                    return true;
            }
        return false;
    }
};
}  // namespace stock_tools
