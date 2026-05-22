#include "yvc/reconstructor.h"
#include "yvc/compressor.h"
#include "yvc/format.h"
#include "yvc/parser.h"
#include "yvc/chunker.h"
#include "yvc/vectorizer.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace yvc {

bool ReconstructionEngine::reconstruct_from_yvc(const std::string& yvc_path,
                                                 const std::string& output_path) {
    CompressedPayload payload;
    if (!YVCDecoder::decode(yvc_path, payload)) {
        std::cerr << "[Reconstruct] Failed to decode YVC file.\n";
        return false;
    }

    VecList vectors = CompressionPipeline::decompress(payload);
    std::cout << "[Reconstruct] Decompressed " << vectors.size() << " vectors\n";

    FileType type = payload.metadata.file_type;
    if (type == FileType::IMAGE_PNG || type == FileType::IMAGE_JPG) {
        ImageData img = reconstruct_image(vectors, payload.metadata);
        if (!ImageParser::save(output_path, img)) {
            std::cerr << "[Reconstruct] Failed to save image: " << output_path << "\n";
            return false;
        }
        std::cout << "[Reconstruct] Image saved: " << output_path
                  << " (" << img.width << "x" << img.height << ")\n";
    } else {
        std::cerr << "[Reconstruct] Unsupported file type for reconstruction.\n";
        return false;
    }

    return true;
}

ImageData ReconstructionEngine::reconstruct_image(const VecList& vectors,
                                                   const FileMetadata& meta) {
    int patch_size = meta.patch_size;
    int channels = meta.channels;

    std::vector<ImagePatch> patches;
    patches.reserve(vectors.size());
    for (const auto& v : vectors)
        patches.push_back(VectorEngine::devectorize_patch(v, patch_size, patch_size, channels));

    return ChunkGenerator::reassemble_patches(patches, meta.width, meta.height, channels);
}

QualityMetrics ReconstructionEngine::measure_quality(const ImageData& original,
                                                      const ImageData& reconstructed) {
    QualityMetrics metrics;
    if (original.pixels.size() != reconstructed.pixels.size()) return metrics;

    size_t n = original.pixels.size();
    double sum_sq = 0;
    for (size_t i = 0; i < n; i++) {
        double diff = (double)original.pixels[i] - (double)reconstructed.pixels[i];
        sum_sq += diff * diff;
    }
    metrics.mse = sum_sq / n;
    metrics.psnr = metrics.mse > 0 ? 10.0 * std::log10(255.0 * 255.0 / metrics.mse) : 100.0;

    double mean_o = 0, mean_r = 0;
    for (size_t i = 0; i < n; i++) { mean_o += original.pixels[i]; mean_r += reconstructed.pixels[i]; }
    mean_o /= n; mean_r /= n;

    double var_o = 0, var_r = 0, cov = 0;
    for (size_t i = 0; i < n; i++) {
        double do_ = original.pixels[i] - mean_o;
        double dr_ = reconstructed.pixels[i] - mean_r;
        var_o += do_ * do_; var_r += dr_ * dr_; cov += do_ * dr_;
    }
    var_o /= n; var_r /= n; cov /= n;

    double c1 = 6.5025, c2 = 58.5225;
    metrics.ssim = ((2*mean_o*mean_r + c1) * (2*cov + c2)) /
                   ((mean_o*mean_o + mean_r*mean_r + c1) * (var_o + var_r + c2));
    return metrics;
}

double ReconstructionEngine::compression_ratio(uint64_t original_bytes, uint64_t compressed_bytes) {
    if (compressed_bytes == 0) return 0;
    return (double)original_bytes / compressed_bytes;
}

} // namespace yvc
