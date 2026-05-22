#pragma once
#include "yvc/types.h"

namespace yvc {

class VectorEngine {
public:
    // Flatten image patch pixels (normalized 0-1) into a vector
    static Vec vectorize_patch(const ImagePatch& patch);

    // Batch vectorize
    static VecList vectorize_patches(const std::vector<ImagePatch>& patches);

    // Reconstruct patch pixel data from a vector
    static ImagePatch devectorize_patch(const Vec& v, int patch_w, int patch_h, int channels);
};

} // namespace yvc
