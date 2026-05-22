#include "yvc/compressor.h"
#include <random>
#include <numeric>
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

namespace yvc {

// ═══════════════════════════════════════════════════════════
// Quantizer
// ═══════════════════════════════════════════════════════════

void Quantizer::quantize(const VecList& vectors, int bits,
                         std::vector<std::vector<uint8_t>>& out_quantized,
                         std::vector<QuantizationParams>& out_params) {
    if (vectors.empty()) return;
    int dim = (int)vectors[0].size();
    int max_val = (1 << bits) - 1;

    // Compute per-dimension min/max
    out_params.resize(dim);
    for (int d = 0; d < dim; d++) {
        float mn = std::numeric_limits<float>::max();
        float mx = std::numeric_limits<float>::lowest();
        for (const auto& v : vectors) {
            mn = std::min(mn, v[d]);
            mx = std::max(mx, v[d]);
        }
        if (mx - mn < 1e-8f) mx = mn + 1e-8f; // avoid division by zero
        out_params[d] = {mn, mx, bits};
    }

    // Quantize each vector
    out_quantized.resize(vectors.size());
    for (size_t i = 0; i < vectors.size(); i++) {
        out_quantized[i].resize(dim);
        for (int d = 0; d < dim; d++) {
            float normalized = (vectors[i][d] - out_params[d].min_val) /
                               (out_params[d].max_val - out_params[d].min_val);
            normalized = std::max(0.0f, std::min(1.0f, normalized));
            out_quantized[i][d] = (uint8_t)(normalized * max_val + 0.5f);
        }
    }
}

VecList Quantizer::dequantize(const std::vector<std::vector<uint8_t>>& quantized,
                              const std::vector<QuantizationParams>& params) {
    VecList result(quantized.size());
    for (size_t i = 0; i < quantized.size(); i++) {
        int dim = (int)quantized[i].size();
        result[i].resize(dim);
        for (int d = 0; d < dim; d++) {
            int max_val = (1 << params[d].bits) - 1;
            float normalized = (float)quantized[i][d] / max_val;
            result[i][d] = params[d].min_val + normalized * (params[d].max_val - params[d].min_val);
        }
    }
    return result;
}

// ═══════════════════════════════════════════════════════════
// K-Means Clustering
// ═══════════════════════════════════════════════════════════

static float vec_dist_sq(const Vec& a, const Vec& b) {
    float sum = 0;
    for (size_t i = 0; i < a.size(); i++) { float d = a[i] - b[i]; sum += d * d; }
    return sum;
}

ClusterResult Clusterer::cluster(const VecList& vectors, int k, int max_iter) {
    if (vectors.empty() || k <= 0) return {};
    int n = (int)vectors.size();
    int dim = (int)vectors[0].size();
    k = std::min(k, n); // can't have more clusters than vectors

    ClusterResult result;
    result.k = k;

    // K-means++ initialization
    std::mt19937 rng(42);
    result.centroids.resize(k);
    std::uniform_int_distribution<int> uniform(0, n - 1);
    result.centroids[0] = vectors[uniform(rng)];

    std::vector<float> min_dists(n, std::numeric_limits<float>::max());
    for (int c = 1; c < k; c++) {
        // Update distances
        for (int i = 0; i < n; i++) {
            float d = vec_dist_sq(vectors[i], result.centroids[c-1]);
            min_dists[i] = std::min(min_dists[i], d);
        }
        // Weighted random selection
        float total = 0;
        for (float d : min_dists) total += d;
        std::uniform_real_distribution<float> urd(0.0f, total);
        float target = urd(rng);
        float cumsum = 0;
        int chosen = 0;
        for (int i = 0; i < n; i++) {
            cumsum += min_dists[i];
            if (cumsum >= target) { chosen = i; break; }
        }
        result.centroids[c] = vectors[chosen];
    }

    // Iterate
    result.assignments.resize(n, 0);
    for (int iter = 0; iter < max_iter; iter++) {
        // Assign to nearest centroid
        bool changed = false;
        for (int i = 0; i < n; i++) {
            float best_dist = std::numeric_limits<float>::max();
            int best_c = 0;
            for (int c = 0; c < k; c++) {
                float d = vec_dist_sq(vectors[i], result.centroids[c]);
                if (d < best_dist) { best_dist = d; best_c = c; }
            }
            if (result.assignments[i] != best_c) { result.assignments[i] = best_c; changed = true; }
        }
        if (!changed) break;

        // Update centroids
        std::vector<int> counts(k, 0);
        VecList new_centroids(k, Vec(dim, 0.0f));
        for (int i = 0; i < n; i++) {
            int c = result.assignments[i];
            counts[c]++;
            for (int d = 0; d < dim; d++) new_centroids[c][d] += vectors[i][d];
        }
        for (int c = 0; c < k; c++) {
            if (counts[c] > 0)
                for (int d = 0; d < dim; d++) new_centroids[c][d] /= counts[c];
            else
                new_centroids[c] = result.centroids[c]; // keep old if empty
        }
        result.centroids = std::move(new_centroids);
    }

    // Compute residuals
    result.residuals.resize(n);
    for (int i = 0; i < n; i++) {
        result.residuals[i].resize(dim);
        int c = result.assignments[i];
        for (int d = 0; d < dim; d++)
            result.residuals[i][d] = vectors[i][d] - result.centroids[c][d];
    }
    return result;
}

VecList Clusterer::reconstruct(const ClusterResult& result) {
    int n = (int)result.assignments.size();
    int dim = result.centroids.empty() ? 0 : (int)result.centroids[0].size();
    VecList out(n);
    for (int i = 0; i < n; i++) {
        out[i].resize(dim);
        int c = result.assignments[i];
        for (int d = 0; d < dim; d++)
            out[i][d] = result.centroids[c][d] + result.residuals[i][d];
    }
    return out;
}

// ═══════════════════════════════════════════════════════════
// Delta Encoding
// ═══════════════════════════════════════════════════════════

void DeltaEncoder::encode(VecList& vectors, Vec& out_base) {
    if (vectors.empty()) return;
    out_base = vectors[0]; // store first vector as base
    for (size_t i = vectors.size() - 1; i >= 1; i--) {
        for (size_t d = 0; d < vectors[i].size(); d++)
            vectors[i][d] -= vectors[i-1][d];
    }
    // vectors[0] stays as-is (or zeroed — we'll keep it as diff from base = 0)
    // Actually keep vectors[0] as the base itself, mark delta from 0
}

void DeltaEncoder::decode(VecList& vectors, const Vec& base) {
    if (vectors.empty()) return;
    (void)base; // vectors[0] is already the base
    for (size_t i = 1; i < vectors.size(); i++) {
        for (size_t d = 0; d < vectors[i].size(); d++)
            vectors[i][d] += vectors[i-1][d];
    }
}

// ═══════════════════════════════════════════════════════════
// PCA Dimensionality Reduction (power iteration)
// ═══════════════════════════════════════════════════════════

PCAResult DimReducer::reduce(const VecList& vectors, int target_dim) {
    PCAResult result;
    if (vectors.empty()) return result;
    int n = (int)vectors.size();
    int dim = (int)vectors[0].size();
    target_dim = std::min(target_dim, std::min(dim, n));
    result.original_dim = dim;
    result.reduced_dim = target_dim;

    // Compute mean
    result.mean.assign(dim, 0.0f);
    for (const auto& v : vectors)
        for (int d = 0; d < dim; d++) result.mean[d] += v[d];
    for (int d = 0; d < dim; d++) result.mean[d] /= n;

    // Center data
    VecList centered(n, Vec(dim));
    for (int i = 0; i < n; i++)
        for (int d = 0; d < dim; d++) centered[i][d] = vectors[i][d] - result.mean[d];

    // Power iteration for top-k eigenvectors of X^T X
    // We use the covariance approach: C = (1/n) X^T X (dim x dim)
    // For each principal component, iterate and deflate
    result.components.resize(target_dim, Vec(dim));
    std::mt19937 rng(123);
    std::normal_distribution<float> normal(0, 1);

    VecList deflated = centered; // work copy for deflation

    for (int pc = 0; pc < target_dim; pc++) {
        // Initialize random vector
        Vec w(dim);
        for (int d = 0; d < dim; d++) w[d] = normal(rng);

        // Power iteration: w = C * w, normalize, repeat
        for (int iter = 0; iter < 100; iter++) {
            // Compute X^T * (X * w) efficiently
            // First: t = X * w  (n-vector)
            std::vector<float> t(n, 0.0f);
            for (int i = 0; i < n; i++)
                for (int d = 0; d < dim; d++) t[i] += deflated[i][d] * w[d];
            // Then: w_new = X^T * t  (dim-vector)
            Vec w_new(dim, 0.0f);
            for (int i = 0; i < n; i++)
                for (int d = 0; d < dim; d++) w_new[d] += deflated[i][d] * t[i];
            // Normalize
            float norm = 0;
            for (float v : w_new) norm += v * v;
            norm = std::sqrt(norm);
            if (norm < 1e-10f) break;
            for (float& v : w_new) v /= norm;
            w = w_new;
        }
        result.components[pc] = w;

        // Deflate: remove component from data
        for (int i = 0; i < n; i++) {
            float proj = 0;
            for (int d = 0; d < dim; d++) proj += deflated[i][d] * w[d];
            for (int d = 0; d < dim; d++) deflated[i][d] -= proj * w[d];
        }
    }

    // Project data onto components
    result.projected.resize(n, Vec(target_dim));
    for (int i = 0; i < n; i++)
        for (int pc = 0; pc < target_dim; pc++) {
            float proj = 0;
            for (int d = 0; d < dim; d++)
                proj += centered[i][d] * result.components[pc][d];
            result.projected[i][pc] = proj;
        }

    return result;
}

VecList DimReducer::reconstruct(const PCAResult& result) {
    int n = (int)result.projected.size();
    VecList out(n, Vec(result.original_dim, 0.0f));
    for (int i = 0; i < n; i++) {
        // Reconstruct: x_approx = mean + sum(proj[k] * component[k])
        for (int d = 0; d < result.original_dim; d++) out[i][d] = result.mean[d];
        for (int pc = 0; pc < result.reduced_dim; pc++)
            for (int d = 0; d < result.original_dim; d++)
                out[i][d] += result.projected[i][pc] * result.components[pc][d];
    }
    return out;
}

// ═══════════════════════════════════════════════════════════
// Full Compression Pipeline
// ═══════════════════════════════════════════════════════════

CompressedPayload CompressionPipeline::compress(const VecList& raw_vectors,
                                                 const CompressionParams& params,
                                                 const FileMetadata& metadata) {
    CompressedPayload payload;
    payload.params = params;
    payload.metadata = metadata;

    VecList working = raw_vectors;
    std::cout << "[Pipeline] Input: " << working.size() << " vectors, dim="
              << (working.empty() ? 0 : working[0].size()) << "\n";

    // Step 1: PCA (optional)
    if (params.use_pca && params.target_dim > 0 && !working.empty() &&
        params.target_dim < (int)working[0].size()) {
        std::cout << "[Pipeline] PCA: " << working[0].size() << "D -> " << params.target_dim << "D\n";
        payload.pca_data = DimReducer::reduce(working, params.target_dim);
        working = payload.pca_data.projected;
    }

    payload.metadata.vector_dim = working.empty() ? 0 : (int)working[0].size();

    // Step 2: VQ-only path (clustering enabled)
    // Store only k centroids + assignment indices — NOT N residual vectors.
    // This is the primary compression: patches -> centroid lookup.
    if (params.use_clustering && params.num_clusters > 0 &&
        (int)working.size() > params.num_clusters) {
        std::cout << "[Pipeline] K-Means VQ: k=" << params.num_clusters << "\n";
        payload.cluster_data = Clusterer::cluster(working, params.num_clusters);
        payload.cluster_data.residuals.clear(); // not stored — pure VQ

        // Quantize the k centroids (not N patch vectors)
        std::cout << "[Pipeline] Quantizing " << payload.cluster_data.centroids.size()
                  << " centroids (" << params.quantization_bits << "-bit)\n";
        Quantizer::quantize(payload.cluster_data.centroids, params.quantization_bits,
                            payload.quantized_vectors, payload.quant_params);

        std::cout << "[Pipeline] Compression complete (VQ mode).\n";
        return payload;
    }

    // Step 3: Delta encoding (optional, non-VQ path)
    if (params.use_delta && working.size() > 1) {
        std::cout << "[Pipeline] Delta encoding\n";
        DeltaEncoder::encode(working, payload.delta_base);
        payload.delta_encoded = true;
    }

    // Step 4: Quantization (non-VQ path)
    std::cout << "[Pipeline] Quantization: " << params.quantization_bits << "-bit\n";
    Quantizer::quantize(working, params.quantization_bits,
                        payload.quantized_vectors, payload.quant_params);

    std::cout << "[Pipeline] Compression complete.\n";
    return payload;
}

VecList CompressionPipeline::decompress(const CompressedPayload& payload) {
    // VQ path: quantized_vectors holds k centroids, assignments map patches -> centroids
    if (payload.params.use_clustering && !payload.cluster_data.assignments.empty()) {
        // Dequantize the k centroids
        VecList centroids = Quantizer::dequantize(payload.quantized_vectors, payload.quant_params);

        // Reconstruct N patch vectors by centroid lookup
        int n = (int)payload.cluster_data.assignments.size();
        VecList working(n);
        for (int i = 0; i < n; i++) {
            int c = payload.cluster_data.assignments[i];
            working[i] = centroids[c];
        }

        // Un-PCA if used
        if (payload.params.use_pca && !payload.pca_data.components.empty()) {
            PCAResult pca_copy = payload.pca_data;
            pca_copy.projected = working;
            working = DimReducer::reconstruct(pca_copy);
        }
        return working;
    }

    // Non-VQ path: standard reverse pipeline
    VecList working = Quantizer::dequantize(payload.quantized_vectors, payload.quant_params);

    if (payload.delta_encoded && working.size() > 1)
        DeltaEncoder::decode(working, payload.delta_base);

    if (payload.params.use_pca && !payload.pca_data.components.empty()) {
        PCAResult pca_copy = payload.pca_data;
        pca_copy.projected = working;
        working = DimReducer::reconstruct(pca_copy);
    }

    return working;
}

} // namespace yvc
