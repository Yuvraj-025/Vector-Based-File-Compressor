#pragma once
#include "yvc/types.h"

namespace yvc {

class ChunkGenerator {
public:
    // Split image into non-overlapping patches
    static std::vector<ImagePatch> generate_patches(const ImageData& img, int patch_size);

    // Split text into fixed-size blocks
    // (kept for potential future use)

    // Reassemble patches into an image
    static ImageData reassemble_patches(const std::vector<ImagePatch>& patches,
                                        int img_width, int img_height, int channels);
};

} // namespace yvc
