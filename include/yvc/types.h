#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <chrono>

namespace yvc {

// ── Core aliases ──
using Vec = std::vector<float>;
using VecList = std::vector<Vec>;

// ── Enums ──
enum class FileType : uint8_t {
    UNKNOWN = 0, IMAGE_PNG = 1, IMAGE_JPG = 2
};

// ── Data Structures ──
struct ImageData {
    std::vector<uint8_t> pixels; // RGB interleaved
    int width = 0, height = 0, channels = 3;
};

struct ImagePatch {
    std::vector<uint8_t> pixels;
    int x = 0, y = 0, width = 0, height = 0, channels = 3;
};


struct FileMetadata {
    std::string original_filename;
    FileType file_type = FileType::UNKNOWN;
    uint64_t original_size = 0;
    int width = 0, height = 0, channels = 3;
    int patch_size = 16;
    int num_chunks = 0;
    int vector_dim = 0;
    uint64_t timestamp = 0;
};

struct QuantizationParams {
    float min_val = 0.0f, max_val = 1.0f;
    int bits = 8;
};

struct ClusterResult {
    VecList centroids;
    std::vector<int> assignments;
    VecList residuals;          // vec - centroid[assignment]
    int k = 0;
};

struct PCAResult {
    VecList components;         // each is a principal axis (length = original_dim)
    Vec mean;
    VecList projected;          // data in reduced space
    int original_dim = 0, reduced_dim = 0;
};

struct CompressionParams {
    int patch_size = 16;
    int quantization_bits = 8;
    int num_clusters = 64;
    int target_dim = 0;         // 0 = no PCA
    bool use_delta = true;
    bool use_clustering = true;
    bool use_pca = false;
    std::string creator_name = "YVC Compressor";
};

struct CompressedPayload {
    std::vector<std::vector<uint8_t>> quantized_vectors;
    std::vector<QuantizationParams> quant_params; // per-dimension
    ClusterResult cluster_data;
    PCAResult pca_data;
    Vec delta_base;
    bool delta_encoded = false;
    CompressionParams params;
    FileMetadata metadata;
};

struct QualityMetrics {
    double mse = 0, psnr = 0, ssim = 0;
    double compression_ratio = 0;
    uint64_t original_size = 0, compressed_size = 0;
};

// ── Utilities ──
inline std::string file_type_str(FileType t) {
    switch (t) {
        case FileType::IMAGE_PNG: return "PNG";
        case FileType::IMAGE_JPG: return "JPG";
        default: return "UNKNOWN";
    }
}

inline FileType detect_file_type(const std::string& path) {
    auto p = path.rfind('.');
    if (p == std::string::npos) return FileType::UNKNOWN;
    std::string ext = path.substr(p);
    for (auto& c : ext) c = (char)std::tolower(c);
    if (ext == ".png") return FileType::IMAGE_PNG;
    if (ext == ".jpg" || ext == ".jpeg") return FileType::IMAGE_JPG;
    return FileType::UNKNOWN;
}

inline uint64_t current_timestamp() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace yvc
