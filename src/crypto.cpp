#include "yvc/crypto.h"
#include <cstring>

namespace yvc {

// ── SHA-256 Constants ──
static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t ep0(uint32_t x) { return rotr(x,2) ^ rotr(x,13) ^ rotr(x,22); }
static inline uint32_t ep1(uint32_t x) { return rotr(x,6) ^ rotr(x,11) ^ rotr(x,25); }
static inline uint32_t sig0(uint32_t x) { return rotr(x,7) ^ rotr(x,18) ^ (x >> 3); }
static inline uint32_t sig1(uint32_t x) { return rotr(x,17) ^ rotr(x,19) ^ (x >> 10); }

SHA256::SHA256() : total_(0), buf_len_(0) {
    state_[0]=0x6a09e667; state_[1]=0xbb67ae85; state_[2]=0x3c6ef372; state_[3]=0xa54ff53a;
    state_[4]=0x510e527f; state_[5]=0x9b05688c; state_[6]=0x1f83d9ab; state_[7]=0x5be0cd19;
    std::memset(buffer_, 0, 64);
}

void SHA256::process_block(const uint8_t* block) {
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|
               ((uint32_t)block[i*4+2]<<8)|((uint32_t)block[i*4+3]);
    for (int i = 16; i < 64; i++)
        w[i] = sig1(w[i-2]) + w[i-7] + sig0(w[i-15]) + w[i-16];

    a=state_[0]; b=state_[1]; c=state_[2]; d=state_[3];
    e=state_[4]; f=state_[5]; g=state_[6]; h=state_[7];

    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + ep1(e) + ch(e,f,g) + K[i] + w[i];
        uint32_t t2 = ep0(a) + maj(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state_[0]+=a; state_[1]+=b; state_[2]+=c; state_[3]+=d;
    state_[4]+=e; state_[5]+=f; state_[6]+=g; state_[7]+=h;
}

void SHA256::update(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buffer_[buf_len_++] = data[i];
        if (buf_len_ == 64) { process_block(buffer_); total_ += 512; buf_len_ = 0; }
    }
}

std::array<uint8_t, 32> SHA256::finalize() {
    total_ += buf_len_ * 8;
    buffer_[buf_len_++] = 0x80;
    if (buf_len_ > 56) {
        while (buf_len_ < 64) buffer_[buf_len_++] = 0;
        process_block(buffer_); buf_len_ = 0;
    }
    while (buf_len_ < 56) buffer_[buf_len_++] = 0;
    for (int i = 7; i >= 0; i--) buffer_[56 + (7-i)] = (uint8_t)(total_ >> (i*8));
    process_block(buffer_);

    std::array<uint8_t, 32> digest;
    for (int i = 0; i < 8; i++) {
        digest[i*4]   = (uint8_t)(state_[i]>>24);
        digest[i*4+1] = (uint8_t)(state_[i]>>16);
        digest[i*4+2] = (uint8_t)(state_[i]>>8);
        digest[i*4+3] = (uint8_t)(state_[i]);
    }
    return digest;
}

std::array<uint8_t, 32> SHA256::hash(const uint8_t* data, size_t len) {
    SHA256 h; h.update(data, len); return h.finalize();
}

std::array<uint8_t, 32> SHA256::hash(const std::vector<uint8_t>& data) {
    return hash(data.data(), data.size());
}

// ── HMAC-SHA256 ──
std::array<uint8_t, 32> HMAC_SHA256::compute(
    const uint8_t* key, size_t key_len,
    const uint8_t* data, size_t data_len)
{
    uint8_t k_pad[64];
    std::memset(k_pad, 0, 64);

    if (key_len > 64) {
        auto hk = SHA256::hash(key, key_len);
        std::memcpy(k_pad, hk.data(), 32);
    } else {
        std::memcpy(k_pad, key, key_len);
    }

    // Inner hash: SHA256(k_ipad || data)
    uint8_t i_pad[64], o_pad[64];
    for (int i = 0; i < 64; i++) { i_pad[i] = k_pad[i] ^ 0x36; o_pad[i] = k_pad[i] ^ 0x5c; }

    SHA256 inner;
    inner.update(i_pad, 64);
    inner.update(data, data_len);
    auto inner_hash = inner.finalize();

    SHA256 outer;
    outer.update(o_pad, 64);
    outer.update(inner_hash.data(), 32);
    return outer.finalize();
}

std::array<uint8_t, 32> HMAC_SHA256::compute(
    const std::string& key, const std::vector<uint8_t>& data)
{
    return compute((const uint8_t*)key.data(), key.size(), data.data(), data.size());
}

} // namespace yvc
