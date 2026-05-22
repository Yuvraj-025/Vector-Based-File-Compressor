#pragma once
#include "yvc/types.h"
#include <cstdint>
#include <string>

namespace yvc {

// ── YVC Format Constants ──
constexpr uint8_t  YVC_MAGIC[8] = {'Y','U','V','C','O','M','P','\0'};
constexpr uint16_t YVC_VERSION  = 0x0100; // v1.0
constexpr size_t   YVC_CREATOR_LEN   = 64;
constexpr size_t   YVC_FILENAME_LEN  = 256;
constexpr size_t   YVC_HMAC_LEN      = 32;

// ── YVC Encoder ──
class YVCEncoder {
public:
    // Encode compressed payload to .yvc file
    static bool encode(const CompressedPayload& payload, const std::string& output_path);
};

// ── YVC Decoder ──
class YVCDecoder {
public:
    // Decode .yvc file back to compressed payload
    // Returns false if file is invalid/tampered
    static bool decode(const std::string& yvc_path, CompressedPayload& out_payload);

    // Read only metadata + creator (no decompression)
    static bool read_info(const std::string& yvc_path,
                          FileMetadata& out_meta,
                          std::string& out_creator,
                          CompressionParams& out_params);
};

} // namespace yvc
