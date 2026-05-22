#include "yvc/chunker.h"
#include <algorithm>
#include <cstring>

namespace yvc {

std::vector<ImagePatch> ChunkGenerator::generate_patches(const ImageData& img, int patch_size) {
    std::vector<ImagePatch> patches;
    int ch = img.channels;

    int nx = (img.width + patch_size - 1) / patch_size;
    int ny = (img.height + patch_size - 1) / patch_size;
    patches.reserve(nx * ny);

    for (int py = 0; py < ny; py++) {
        for (int px = 0; px < nx; px++) {
            ImagePatch patch;
            patch.x = px * patch_size;
            patch.y = py * patch_size;
            patch.width = std::min(patch_size, img.width - patch.x);
            patch.height = std::min(patch_size, img.height - patch.y);
            patch.channels = ch;

            // Always store as full patch_size x patch_size (zero-pad edges)
            patch.pixels.resize((size_t)patch_size * patch_size * ch, 0);

            for (int row = 0; row < patch.height; row++) {
                for (int col = 0; col < patch.width; col++) {
                    size_t src_idx = ((size_t)(patch.y + row) * img.width + (patch.x + col)) * ch;
                    size_t dst_idx = ((size_t)row * patch_size + col) * ch;
                    for (int c = 0; c < ch; c++)
                        patch.pixels[dst_idx + c] = img.pixels[src_idx + c];
                }
            }

            patch.width = patch_size;
            patch.height = patch_size;
            patches.push_back(std::move(patch));
        }
    }
    return patches;
}

ImageData ChunkGenerator::reassemble_patches(const std::vector<ImagePatch>& patches,
                                              int img_width, int img_height, int channels) {
    ImageData img;
    img.width = img_width;
    img.height = img_height;
    img.channels = channels;
    img.pixels.resize((size_t)img_width * img_height * channels, 0);

    if (patches.empty()) return img;

    int patch_size = patches[0].width;
    int nx = (img_width + patch_size - 1) / patch_size;

    for (size_t pi = 0; pi < patches.size(); pi++) {
        int px = (int)(pi % nx) * patch_size;
        int py = (int)(pi / nx) * patch_size;
        int pw = std::min(patch_size, img_width - px);
        int ph = std::min(patch_size, img_height - py);

        for (int row = 0; row < ph; row++) {
            for (int col = 0; col < pw; col++) {
                size_t src_idx = ((size_t)row * patch_size + col) * channels;
                size_t dst_idx = ((size_t)(py + row) * img_width + (px + col)) * channels;
                for (int c = 0; c < channels; c++)
                    img.pixels[dst_idx + c] = patches[pi].pixels[src_idx + c];
            }
        }
    }
    return img;
}

} // namespace yvc
