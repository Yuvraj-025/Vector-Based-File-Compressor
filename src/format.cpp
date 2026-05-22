#include "yvc/format.h"
#include "yvc/crypto.h"
#include <fstream>
#include <cstring>
#include <iostream>
#include <cmath>

namespace yvc {

// ── Helper: write/read POD values ──
template<typename T>
static void write_val(std::ofstream& f, T val) { f.write(reinterpret_cast<const char*>(&val), sizeof(T)); }
template<typename T>
static void read_val(std::ifstream& f, T& val) { f.read(reinterpret_cast<char*>(&val), sizeof(T)); }

static void write_str(std::ofstream& f, const std::string& s, size_t fixed_len) {
    char buf[256] = {};
    size_t copy_len = std::min(s.size(), fixed_len - 1);
    std::memcpy(buf, s.data(), copy_len);
    f.write(buf, (std::streamsize)fixed_len);
}

static std::string read_str(std::ifstream& f, size_t fixed_len) {
    std::vector<char> buf(fixed_len, 0);
    f.read(buf.data(), (std::streamsize)fixed_len);
    return std::string(buf.data());
}

static void write_vec(std::ofstream& f, const Vec& v) {
    int32_t sz = (int32_t)v.size();
    write_val(f, sz);
    f.write(reinterpret_cast<const char*>(v.data()), sz * sizeof(float));
}

static Vec read_vec(std::ifstream& f) {
    int32_t sz; read_val(f, sz);
    Vec v(sz);
    f.read(reinterpret_cast<char*>(v.data()), sz * sizeof(float));
    return v;
}

// Bit-pack assignment indices: ceil(log2(k)) bits per index
static void write_assignments_packed(std::ofstream& f, const std::vector<int>& assignments, int k) {
    int bits = 1;
    while ((1 << bits) < k) bits++;
    uint32_t buf = 0;
    int bits_in_buf = 0;
    for (int a : assignments) {
        buf |= ((uint32_t)a << bits_in_buf);
        bits_in_buf += bits;
        while (bits_in_buf >= 8) {
            f.put((char)(buf & 0xFF));
            buf >>= 8;
            bits_in_buf -= 8;
        }
    }
    if (bits_in_buf > 0) f.put((char)(buf & 0xFF));
}

static std::vector<int> read_assignments_packed(std::ifstream& f, int n, int k) {
    int bits = 1;
    while ((1 << bits) < k) bits++;
    int mask = (1 << bits) - 1;
    std::vector<int> result(n);
    uint32_t buf = 0;
    int bits_in_buf = 0;
    for (int i = 0; i < n; i++) {
        while (bits_in_buf < bits) {
            uint8_t byte;
            f.read(reinterpret_cast<char*>(&byte), 1);
            buf |= ((uint32_t)byte << bits_in_buf);
            bits_in_buf += 8;
        }
        result[i] = buf & mask;
        buf >>= bits;
        bits_in_buf -= bits;
    }
    return result;
}

// Collect all bytes from current position to end for HMAC
static std::vector<uint8_t> collect_payload_bytes(std::ifstream& f, std::streampos start, std::streampos end) {
    auto cur = f.tellg();
    f.seekg(start);
    size_t len = (size_t)(end - start);
    std::vector<uint8_t> data(len);
    f.read(reinterpret_cast<char*>(data.data()), len);
    f.seekg(cur);
    return data;
}

// ═══════════════════════════════════════════════════════════
// YVC Encoder
// ═══════════════════════════════════════════════════════════

bool YVCEncoder::encode(const CompressedPayload& payload, const std::string& output_path) {
    std::ofstream f(output_path, std::ios::binary);
    if (!f.is_open()) { std::cerr << "Cannot open output: " << output_path << "\n"; return false; }

    // ── Magic + Version ──
    f.write(reinterpret_cast<const char*>(YVC_MAGIC), 8);
    write_val(f, YVC_VERSION);

    // ── Creator Tag ──
    write_str(f, payload.params.creator_name, YVC_CREATOR_LEN);

    // ── Header HMAC placeholder (fill later) ──
    auto hmac_pos = f.tellp();
    uint8_t zero_hmac[32] = {};
    f.write(reinterpret_cast<const char*>(zero_hmac), 32);

    // ── Metadata ──
    auto payload_start = f.tellp();
    write_str(f, payload.metadata.original_filename, YVC_FILENAME_LEN);
    write_val(f, (uint8_t)payload.metadata.file_type);
    write_val(f, payload.metadata.original_size);
    write_val(f, (int32_t)payload.metadata.width);
    write_val(f, (int32_t)payload.metadata.height);
    write_val(f, (int32_t)payload.metadata.channels);
    write_val(f, (int32_t)payload.metadata.patch_size);
    write_val(f, (int32_t)payload.metadata.num_chunks);
    write_val(f, (int32_t)payload.metadata.vector_dim);
    write_val(f, payload.metadata.timestamp);

    // ── Compression Params ──
    write_val(f, (int32_t)payload.params.quantization_bits);
    write_val(f, (int32_t)payload.params.num_clusters);
    write_val(f, (int32_t)payload.params.target_dim);
    write_val(f, (uint8_t)payload.params.use_delta);
    write_val(f, (uint8_t)payload.params.use_clustering);
    write_val(f, (uint8_t)payload.params.use_pca);
    write_val(f, (uint8_t)payload.delta_encoded);

    // ── PCA Data ──
    write_val(f, (uint8_t)payload.params.use_pca);
    if (payload.params.use_pca && !payload.pca_data.components.empty()) {
        write_val(f, (int32_t)payload.pca_data.original_dim);
        write_val(f, (int32_t)payload.pca_data.reduced_dim);
        write_vec(f, payload.pca_data.mean);
        write_val(f, (int32_t)payload.pca_data.components.size());
        for (const auto& comp : payload.pca_data.components)
            write_vec(f, comp);
    }

    // ── Cluster Data (VQ mode: compact centroids + bit-packed assignments) ──
    write_val(f, (uint8_t)payload.params.use_clustering);
    if (payload.params.use_clustering && !payload.cluster_data.assignments.empty()) {
        int32_t k = payload.cluster_data.k;
        int32_t n_assign = (int32_t)payload.cluster_data.assignments.size();
        write_val(f, k);
        write_val(f, n_assign);

        // Centroids are already quantized in payload.quantized_vectors (k entries)
        // Write quant params (per-dim min/max/bits) then raw centroid bytes
        int32_t n_qp = (int32_t)payload.quant_params.size();
        write_val(f, n_qp);
        for (const auto& qp : payload.quant_params) {
            write_val(f, qp.min_val);
            write_val(f, qp.max_val);
            write_val(f, (int32_t)qp.bits);
        }
        // Raw centroid bytes: k centroids × dim bytes
        for (const auto& qv : payload.quantized_vectors)
            f.write(reinterpret_cast<const char*>(qv.data()), qv.size());

        // Bit-packed assignment indices
        write_assignments_packed(f, payload.cluster_data.assignments, k);
    }

    // ── Delta Base (non-VQ path only) ──
    write_val(f, (uint8_t)payload.delta_encoded);
    if (payload.delta_encoded)
        write_vec(f, payload.delta_base);

    // ── Quantization Params + Vectors (non-VQ path only) ──
    uint8_t vq_mode = payload.params.use_clustering && !payload.cluster_data.assignments.empty() ? 1 : 0;
    write_val(f, vq_mode);
    if (!vq_mode) {
        int32_t n_qparams = (int32_t)payload.quant_params.size();
        write_val(f, n_qparams);
        for (const auto& qp : payload.quant_params) {
            write_val(f, qp.min_val);
            write_val(f, qp.max_val);
            write_val(f, (int32_t)qp.bits);
        }
        int32_t n_vecs = (int32_t)payload.quantized_vectors.size();
        int32_t vec_bytes = n_vecs > 0 ? (int32_t)payload.quantized_vectors[0].size() : 0;
        write_val(f, n_vecs);
        write_val(f, vec_bytes);
        for (const auto& qv : payload.quantized_vectors)
            f.write(reinterpret_cast<const char*>(qv.data()), qv.size());
    }

    auto payload_end = f.tellp();

    // ── Compute and write payload HMAC ──
    // First, read back all payload bytes
    f.flush();
    std::ifstream rf(output_path, std::ios::binary);
    auto payload_bytes = collect_payload_bytes(rf, payload_start, payload_end);
    rf.close();

    auto payload_hmac = HMAC_SHA256::compute(YVC_SECRET_KEY, payload_bytes);
    f.write(reinterpret_cast<const char*>(payload_hmac.data()), 32);

    // ── Compute and write header HMAC ──
    f.flush();
    // Header = magic(8) + version(2) + creator(64) = 74 bytes
    std::ifstream rf2(output_path, std::ios::binary);
    std::vector<uint8_t> header_bytes(74);
    rf2.read(reinterpret_cast<char*>(header_bytes.data()), 74);
    rf2.close();

    auto header_hmac = HMAC_SHA256::compute(YVC_SECRET_KEY, header_bytes);
    f.seekp(hmac_pos);
    f.write(reinterpret_cast<const char*>(header_hmac.data()), 32);

    f.close();
    std::cout << "[YVC] Written: " << output_path << "\n";
    return true;
}

// ═══════════════════════════════════════════════════════════
// YVC Decoder
// ═══════════════════════════════════════════════════════════

bool YVCDecoder::decode(const std::string& yvc_path, CompressedPayload& out) {
    std::ifstream f(yvc_path, std::ios::binary);
    if (!f.is_open()) { std::cerr << "Cannot open: " << yvc_path << "\n"; return false; }

    // ── Magic ──
    uint8_t magic[8];
    f.read(reinterpret_cast<char*>(magic), 8);
    if (std::memcmp(magic, YVC_MAGIC, 8) != 0) {
        std::cerr << "Error: Not a valid YVC file (bad magic bytes).\n"; return false;
    }

    // ── Version ──
    uint16_t version; read_val(f, version);
    if (version != YVC_VERSION) {
        std::cerr << "Error: Unsupported YVC version.\n"; return false;
    }

    // ── Creator ──
    std::string creator = read_str(f, YVC_CREATOR_LEN);

    // ── Header HMAC ──
    std::array<uint8_t, 32> stored_header_hmac;
    f.read(reinterpret_cast<char*>(stored_header_hmac.data()), 32);

    // Verify header HMAC
    {
        std::ifstream rf(yvc_path, std::ios::binary);
        std::vector<uint8_t> header_bytes(74);
        rf.read(reinterpret_cast<char*>(header_bytes.data()), 74);
        rf.close();
        auto computed = HMAC_SHA256::compute(YVC_SECRET_KEY, header_bytes);
        if (computed != stored_header_hmac) {
            std::cerr << "Error: Header HMAC validation failed. File may be tampered.\n";
            return false;
        }
    }

    // ── Metadata ──
    auto payload_start = f.tellg();
    out.metadata.original_filename = read_str(f, YVC_FILENAME_LEN);
    uint8_t ft; read_val(f, ft); out.metadata.file_type = (FileType)ft;
    read_val(f, out.metadata.original_size);
    int32_t w, h, ch, ps, nc, vd;
    read_val(f, w); out.metadata.width = w;
    read_val(f, h); out.metadata.height = h;
    read_val(f, ch); out.metadata.channels = ch;
    read_val(f, ps); out.metadata.patch_size = ps;
    read_val(f, nc); out.metadata.num_chunks = nc;
    read_val(f, vd); out.metadata.vector_dim = vd;
    read_val(f, out.metadata.timestamp);

    // ── Compression Params ──
    int32_t qbits, nclusters, tdim;
    uint8_t udelta, uclust, upca, denc;
    read_val(f, qbits); out.params.quantization_bits = qbits;
    read_val(f, nclusters); out.params.num_clusters = nclusters;
    read_val(f, tdim); out.params.target_dim = tdim;
    read_val(f, udelta); out.params.use_delta = udelta;
    read_val(f, uclust); out.params.use_clustering = uclust;
    read_val(f, upca); out.params.use_pca = upca;
    read_val(f, denc); out.delta_encoded = denc;
    out.params.creator_name = creator;

    // ── PCA Data ──
    uint8_t has_pca; read_val(f, has_pca);
    if (has_pca) {
        int32_t od, rd;
        read_val(f, od); out.pca_data.original_dim = od;
        read_val(f, rd); out.pca_data.reduced_dim = rd;
        out.pca_data.mean = read_vec(f);
        int32_t ncomp; read_val(f, ncomp);
        out.pca_data.components.resize(ncomp);
        for (int i = 0; i < ncomp; i++)
            out.pca_data.components[i] = read_vec(f);
    }

    // ── Cluster Data (VQ mode) ──
    uint8_t has_clust; read_val(f, has_clust);
    if (has_clust) {
        int32_t k, n_assign;
        read_val(f, k); out.cluster_data.k = k;
        read_val(f, n_assign);

        // Read quant params for centroids
        int32_t n_qp; read_val(f, n_qp);
        out.quant_params.resize(n_qp);
        for (int i = 0; i < n_qp; i++) {
            read_val(f, out.quant_params[i].min_val);
            read_val(f, out.quant_params[i].max_val);
            int32_t bits; read_val(f, bits); out.quant_params[i].bits = bits;
        }
        // Raw centroid bytes: k centroids × n_qp bytes each
        int dim = n_qp;
        out.quantized_vectors.resize(k);
        for (int i = 0; i < k; i++) {
            out.quantized_vectors[i].resize(dim);
            f.read(reinterpret_cast<char*>(out.quantized_vectors[i].data()), dim);
        }
        // Bit-packed assignments
        out.cluster_data.assignments = read_assignments_packed(f, n_assign, k);
    }

    // ── Delta Base ──
    uint8_t has_delta; read_val(f, has_delta);
    if (has_delta)
        out.delta_base = read_vec(f);

    // ── Quantization Params + Vectors (non-VQ path only) ──
    uint8_t vq_mode; read_val(f, vq_mode);
    if (!vq_mode) {
        int32_t n_qp; read_val(f, n_qp);
        out.quant_params.resize(n_qp);
        for (int i = 0; i < n_qp; i++) {
            read_val(f, out.quant_params[i].min_val);
            read_val(f, out.quant_params[i].max_val);
            int32_t bits; read_val(f, bits); out.quant_params[i].bits = bits;
        }
        int32_t n_vecs, vec_bytes;
        read_val(f, n_vecs); read_val(f, vec_bytes);
        out.quantized_vectors.resize(n_vecs);
        for (int i = 0; i < n_vecs; i++) {
            out.quantized_vectors[i].resize(vec_bytes);
            f.read(reinterpret_cast<char*>(out.quantized_vectors[i].data()), vec_bytes);
        }
    }

    auto payload_end = f.tellg();

    // ── Payload HMAC ──
    std::array<uint8_t, 32> stored_payload_hmac;
    f.read(reinterpret_cast<char*>(stored_payload_hmac.data()), 32);

    // Verify payload HMAC
    {
        std::ifstream rf(yvc_path, std::ios::binary);
        auto payload_bytes = collect_payload_bytes(rf, payload_start, payload_end);
        rf.close();
        auto computed = HMAC_SHA256::compute(YVC_SECRET_KEY, payload_bytes);
        if (computed != stored_payload_hmac) {
            std::cerr << "Error: Payload HMAC validation failed. File may be corrupted.\n";
            return false;
        }
    }

    std::cout << "[YVC] Decoded: " << yvc_path << " (creator: " << creator << ")\n";
    return true;
}

bool YVCDecoder::read_info(const std::string& yvc_path,
                           FileMetadata& out_meta,
                           std::string& out_creator,
                           CompressionParams& out_params) {
    std::ifstream f(yvc_path, std::ios::binary);
    if (!f.is_open()) return false;

    uint8_t magic[8];
    f.read(reinterpret_cast<char*>(magic), 8);
    if (std::memcmp(magic, YVC_MAGIC, 8) != 0) return false;

    uint16_t version; read_val(f, version);
    out_creator = read_str(f, YVC_CREATOR_LEN);

    // Skip HMAC
    f.seekg(32, std::ios::cur);

    // Read metadata
    out_meta.original_filename = read_str(f, YVC_FILENAME_LEN);
    uint8_t ft; read_val(f, ft); out_meta.file_type = (FileType)ft;
    read_val(f, out_meta.original_size);
    int32_t tmp;
    read_val(f, tmp); out_meta.width = tmp;
    read_val(f, tmp); out_meta.height = tmp;
    read_val(f, tmp); out_meta.channels = tmp;
    read_val(f, tmp); out_meta.patch_size = tmp;
    read_val(f, tmp); out_meta.num_chunks = tmp;
    read_val(f, tmp); out_meta.vector_dim = tmp;
    read_val(f, out_meta.timestamp);

    int32_t qb, nc, td;
    uint8_t ud, uc, up;
    read_val(f, qb); out_params.quantization_bits = qb;
    read_val(f, nc); out_params.num_clusters = nc;
    read_val(f, td); out_params.target_dim = td;
    read_val(f, ud); out_params.use_delta = ud;
    read_val(f, uc); out_params.use_clustering = uc;
    read_val(f, up); out_params.use_pca = up;
    out_params.creator_name = out_creator;

    return true;
}

} // namespace yvc
