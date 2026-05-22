#pragma once
#include "yvc/types.h"

namespace yvc {

// ── Scalar Quantization ──
class Quantizer {
public:
    // Quantize a list of vectors; returns quantized bytes + params per dimension
    static void quantize(const VecList& vectors, int bits,
                         std::vector<std::vector<uint8_t>>& out_quantized,
                         std::vector<QuantizationParams>& out_params);

    // Dequantize back to float vectors
    static VecList dequantize(const std::vector<std::vector<uint8_t>>& quantized,
                              const std::vector<QuantizationParams>& params);
};

// ── K-Means Clustering ──
class Clusterer {
public:
    // Run k-means on vectors, returns cluster result with centroids + assignments + residuals
    static ClusterResult cluster(const VecList& vectors, int k, int max_iter = 50);

    // Reconstruct vectors from cluster result (centroid + residual)
    static VecList reconstruct(const ClusterResult& result);
};

// ── Delta Encoding ──
class DeltaEncoder {
public:
    // Encode: store first vector as base, rest as deltas
    static void encode(VecList& vectors, Vec& out_base);

    // Decode: restore original vectors from base + deltas
    static void decode(VecList& vectors, const Vec& base);
};

// ── PCA Dimensionality Reduction ──
class DimReducer {
public:
    // Reduce dimensionality via PCA
    static PCAResult reduce(const VecList& vectors, int target_dim);

    // Reconstruct approximate original vectors from PCA result
    static VecList reconstruct(const PCAResult& result);
};

// ── Full Compression Pipeline ──
class CompressionPipeline {
public:
    // Compress vectors with configured pipeline
    static CompressedPayload compress(const VecList& raw_vectors,
                                      const CompressionParams& params,
                                      const FileMetadata& metadata);

    // Decompress payload back to vectors
    static VecList decompress(const CompressedPayload& payload);
};

} // namespace yvc
