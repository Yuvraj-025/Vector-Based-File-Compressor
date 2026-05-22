#pragma once
#include "yvc/types.h"
#include <string>

namespace yvc {

class ReconstructionEngine {
public:
    // Full pipeline: .yvc file → reconstructed output file
    static bool reconstruct_from_yvc(const std::string& yvc_path,
                                     const std::string& output_path);

    // Reconstruct image from decompressed vectors
    static ImageData reconstruct_image(const VecList& vectors,
                                       const FileMetadata& meta);

    // Reconstruct text from decompressed vectors
    static std::string reconstruct_text(const VecList& vectors,
                                        const FileMetadata& meta);

    // Compare original and reconstructed image
    static QualityMetrics measure_quality(const ImageData& original,
                                          const ImageData& reconstructed);

    // Measure file-level compression ratio
    static double compression_ratio(uint64_t original_bytes, uint64_t compressed_bytes);
};

} // namespace yvc
