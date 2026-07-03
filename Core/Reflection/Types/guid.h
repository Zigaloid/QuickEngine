#pragma once

#include <array>
#include <string>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

/**
 * GUID128 - simple 128-bit GUID helper.
 * - Binary storage is a std::array<uint8_t,16>.
 * - Generate() produces a UUIDv4-style random GUID (sets version/variant bits).
 * - ToString()/FromString() convert between canonical GUID string "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
 */
struct GUID128
{
    std::array<uint8_t, 16> bytes{};

    static GUID128 Generate()
    {
        GUID128 g;

        // Seed generator with high-quality seed derived from random_device and time
        static std::uint64_t seedFallback = static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());

        std::random_device rd;
        std::uint64_t seed = (static_cast<std::uint64_t>(rd()) << 32) ^ rd() ^ seedFallback;
        // Mix in address of static to increase variance across runs/hosts
        seed ^= reinterpret_cast<std::uint64_t>(&seedFallback);

        thread_local std::mt19937_64 rng(seed);
        std::uint64_t r1 = rng();
        std::uint64_t r2 = rng();

        // Fill bytes
        for (int i = 0; i < 8; ++i)
            g.bytes[i] = static_cast<uint8_t>((r1 >> (8 * i)) & 0xFF);
        for (int i = 0; i < 8; ++i)
            g.bytes[8 + i] = static_cast<uint8_t>((r2 >> (8 * i)) & 0xFF);

        // Set UUIDv4 variant and version bits:
        // version (byte 6): xxxx0100 => set high nibble to 4
        g.bytes[6] = static_cast<uint8_t>((g.bytes[6] & 0x0F) | 0x40);
        // variant (byte 8): 10xxxxxx
        g.bytes[8] = static_cast<uint8_t>((g.bytes[8] & 0x3F) | 0x80);

        return g;
    }

    static std::string ToString(const GUID128& g)
    {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        // 4-2-2-2-6 bytes grouping -> 8-4-4-4-12 hex chars
        auto writeBytes = [&](int start, int count) {
            for (int i = 0; i < count; ++i)
                ss << std::setw(2) << static_cast<int>(g.bytes[start + i]);
        };
        writeBytes(0, 4); // 8 hex
        ss << '-';
        writeBytes(4, 2); // 4 hex
        ss << '-';
        writeBytes(6, 2); // 4 hex
        ss << '-';
        writeBytes(8, 2); // 4 hex
        ss << '-';
        writeBytes(10, 6); // 12 hex
        return ss.str();
    }

    static bool FromString(const std::string& s, GUID128& out)
    {
        // Accept canonical form: 8-4-4-4-12 hex with dashes
        std::string tmp;
        tmp.reserve(32);
        for (char c : s) {
            if (std::isxdigit(static_cast<unsigned char>(c))) tmp.push_back(c);
        }
        if (tmp.size() != 32) return false;

        for (size_t i = 0; i < 16; ++i) {
            unsigned int hi = 0, lo = 0;
            auto hexVal = [](char c)->int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                return 0;
            };
            hi = hexVal(tmp[2 * i]);
            lo = hexVal(tmp[2 * i + 1]);
            out.bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
        }
        return true;
    }
};