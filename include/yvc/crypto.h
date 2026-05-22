#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace yvc {

// ── SHA-256 ──
class SHA256 {
public:
    SHA256();
    void update(const uint8_t* data, size_t len);
    void update(const std::string& s) { update((const uint8_t*)s.data(), s.size()); }
    std::array<uint8_t, 32> finalize();
    static std::array<uint8_t, 32> hash(const uint8_t* data, size_t len);
    static std::array<uint8_t, 32> hash(const std::vector<uint8_t>& data);

private:
    uint32_t state_[8];
    uint8_t  buffer_[64];
    uint64_t total_;
    size_t   buf_len_;
    void process_block(const uint8_t* block);
};

// ── HMAC-SHA256 ──
class HMAC_SHA256 {
public:
    static std::array<uint8_t, 32> compute(
        const uint8_t* key, size_t key_len,
        const uint8_t* data, size_t data_len);
    static std::array<uint8_t, 32> compute(
        const std::string& key, const std::vector<uint8_t>& data);
};

// Embedded secret key for proprietary format validation
const std::string YVC_SECRET_KEY = "YVC-PROPRIETARY-KEY-2026-YUVRAJ-SECURE";

} // namespace yvc
