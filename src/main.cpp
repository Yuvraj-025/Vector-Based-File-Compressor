#include "yvc/types.h"
#include "yvc/parser.h"
#include "yvc/chunker.h"
#include "yvc/vectorizer.h"
#include "yvc/compressor.h"
#include "yvc/format.h"
#include "yvc/reconstructor.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <iomanip>
#include <ctime>

namespace fs = std::filesystem;

static void print_banner() {
    std::cout << R"(
  ╔═══════════════════════════════════════════════╗
  ║   YVC - Vector-Based Semantic Compressor      ║
  ║   Proprietary Format (.yvc)                   ║
  ╚═══════════════════════════════════════════════╝
)" << std::endl;
}

static void print_usage() {
    std::cout << "Usage:\n"
              << "  yvc compress <input> -o <output.yvc> [options]\n"
              << "  yvc decompress <input.yvc> -o <output>\n"
              << "  yvc info <input.yvc>\n\n"
              << "Supported inputs: .png  .jpg  .jpeg\n\n"
              << "Compress options:\n"
              << "  --creator <name>    Creator name (default: YVC Compressor)\n"
              << "  --patch <n>         Patch size (default: 8)\n"
              << "  --clusters <n>      Number of clusters (default: 32)\n"
              << "  --bits <n>          Quantization bits (default: 8)\n"
              << "  --pca <n>           PCA target dims (0 = off, default: 0)\n"
              << "  --no-cluster        Disable clustering\n"
              << std::endl;
}

static int cmd_compress(int argc, char* argv[]) {
    if (argc < 4) { print_usage(); return 1; }

    std::string input_path = argv[2];
    std::string output_path;
    yvc::CompressionParams params;
    params.patch_size = 8;
    params.num_clusters = 32;
    params.use_delta = false; // not used in VQ mode

    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" && i+1 < argc) output_path = argv[++i];
        else if (arg == "--creator" && i+1 < argc) params.creator_name = argv[++i];
        else if (arg == "--patch" && i+1 < argc) params.patch_size = std::stoi(argv[++i]);
        else if (arg == "--clusters" && i+1 < argc) params.num_clusters = std::stoi(argv[++i]);
        else if (arg == "--bits" && i+1 < argc) params.quantization_bits = std::stoi(argv[++i]);
        else if (arg == "--pca" && i+1 < argc) { params.target_dim = std::stoi(argv[++i]); params.use_pca = params.target_dim > 0; }
        else if (arg == "--no-cluster") params.use_clustering = false;
    }

    if (output_path.empty())
        output_path = fs::path(input_path).stem().string() + ".yvc";

    yvc::FileType type = yvc::detect_file_type(input_path);
    if (type == yvc::FileType::UNKNOWN) {
        std::cerr << "Error: Unsupported file type: " << input_path << "\n"
                  << "Supported: .png  .jpg  .jpeg\n";
        return 1;
    }

    std::cout << "[YVC] Compressing: " << input_path << "\n"
              << "[YVC] Output:      " << output_path << "\n"
              << "[YVC] Creator:     " << params.creator_name << "\n"
              << "[YVC] Type:        " << yvc::file_type_str(type) << "\n\n";

    auto img = yvc::ImageParser::load(input_path);
    std::cout << "[YVC] Image:   " << img.width << "x" << img.height << "x" << img.channels << "\n";

    auto patches = yvc::ChunkGenerator::generate_patches(img, params.patch_size);
    std::cout << "[YVC] Patches: " << patches.size()
              << " (" << params.patch_size << "x" << params.patch_size << ")\n";

    auto vectors = yvc::VectorEngine::vectorize_patches(patches);

    yvc::FileMetadata meta;
    meta.original_filename = fs::path(input_path).filename().string();
    meta.file_type = type;
    meta.original_size = fs::file_size(input_path);
    meta.patch_size = params.patch_size;
    meta.width = img.width;
    meta.height = img.height;
    meta.channels = img.channels;
    meta.num_chunks = (int)patches.size();
    meta.vector_dim = vectors.empty() ? 0 : (int)vectors[0].size();
    meta.timestamp = yvc::current_timestamp();

    std::cout << "[YVC] Vectors: " << vectors.size() << " x " << meta.vector_dim << "D\n\n";

    auto payload = yvc::CompressionPipeline::compress(vectors, params, meta);

    if (!yvc::YVCEncoder::encode(payload, output_path)) {
        std::cerr << "Error: Failed to write YVC file.\n";
        return 1;
    }

    uint64_t compressed_size = fs::file_size(output_path);
    double ratio = yvc::ReconstructionEngine::compression_ratio(meta.original_size, compressed_size);
    std::cout << "\n╔══════════════════════════════════════╗\n";
    std::cout << "║  Compression Complete                ║\n";
    std::cout << "╠══════════════════════════════════════╣\n";
    std::cout << "║  Original:   " << std::setw(12) << meta.original_size << " bytes  ║\n";
    std::cout << "║  Compressed: " << std::setw(12) << compressed_size << " bytes  ║\n";
    std::cout << "║  Ratio:      " << std::setw(12) << std::fixed << std::setprecision(2)
              << ratio << "x       ║\n";
    std::cout << "╚══════════════════════════════════════╝\n";
    return 0;
}

static int cmd_decompress(int argc, char* argv[]) {
    if (argc < 3) { print_usage(); return 1; }

    std::string input_path = argv[2];
    std::string output_path;

    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" && i+1 < argc) output_path = argv[++i];
    }

    if (output_path.empty()) {
        yvc::FileMetadata meta;
        std::string creator;
        yvc::CompressionParams params;
        if (yvc::YVCDecoder::read_info(input_path, meta, creator, params))
            output_path = "restored_" + meta.original_filename;
        else
            output_path = "restored_output.png";
    }

    std::cout << "[YVC] Decompressing: " << input_path << "\n"
              << "[YVC] Output:        " << output_path << "\n\n";

    if (!yvc::ReconstructionEngine::reconstruct_from_yvc(input_path, output_path)) {
        std::cerr << "Error: Decompression failed.\n";
        return 1;
    }

    std::cout << "\n[YVC] Decompression complete: " << output_path << "\n";
    return 0;
}

static int cmd_info(int argc, char* argv[]) {
    if (argc < 3) { print_usage(); return 1; }

    std::string input_path = argv[2];
    yvc::FileMetadata meta;
    std::string creator;
    yvc::CompressionParams params;

    if (!yvc::YVCDecoder::read_info(input_path, meta, creator, params)) {
        std::cerr << "Error: Cannot read YVC file info (invalid or tampered file).\n";
        return 1;
    }

    uint64_t file_size = fs::file_size(input_path);
    double ratio = (meta.original_size > 0 && file_size > 0) ?
                   (double)meta.original_size / file_size : 0;

    std::time_t ts = (std::time_t)meta.timestamp;
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&ts));

    std::cout << "╔══════════════════════════════════════════════╗\n";
    std::cout << "║  YVC File Information                        ║\n";
    std::cout << "╠══════════════════════════════════════════════╣\n";
    std::cout << "║  Creator:    " << std::setw(32) << std::left << creator << "║\n";
    std::cout << "║  Original:   " << std::setw(32) << std::left << meta.original_filename << "║\n";
    std::cout << "║  Type:       " << std::setw(32) << std::left << yvc::file_type_str(meta.file_type) << "║\n";
    if (meta.width > 0)
        std::cout << "║  Dimensions: " << std::setw(32) << std::left
                  << (std::to_string(meta.width)+"x"+std::to_string(meta.height)+"x"+std::to_string(meta.channels)) << "║\n";
    std::cout << "║  Original:   " << std::setw(27) << std::left << (std::to_string(meta.original_size)+" bytes") << "     ║\n";
    std::cout << "║  Compressed: " << std::setw(27) << std::left << (std::to_string(file_size)+" bytes") << "     ║\n";
    std::cout << "║  Ratio:      " << std::setw(27) << std::left
              << (std::to_string(ratio).substr(0,5)+"x") << "     ║\n";
    std::cout << "║  Created:    " << std::setw(32) << std::left << time_buf << "║\n";
    std::cout << "║  Patches:    " << std::setw(32) << std::left
              << (std::to_string(meta.num_chunks)+" chunks x "+std::to_string(meta.vector_dim)+"D") << "║\n";
    std::cout << "║  Clusters:   " << std::setw(32) << std::left << params.num_clusters << "║\n";
    std::cout << "║  Quant bits: " << std::setw(32) << std::left << params.quantization_bits << "║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";
    return 0;
}

int main(int argc, char* argv[]) {
    print_banner();
    if (argc < 2) { print_usage(); return 1; }

    std::string command = argv[1];
    if (command == "compress")   return cmd_compress(argc, argv);
    if (command == "decompress") return cmd_decompress(argc, argv);
    if (command == "info")       return cmd_info(argc, argv);
    if (command == "--help" || command == "-h") { print_usage(); return 0; }

    std::cerr << "Unknown command: " << command << "\n";
    print_usage();
    return 1;
}
