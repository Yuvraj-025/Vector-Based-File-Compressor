#include "yvc/vectorizer.h"
#include <cmath>
#include <algorithm>

namespace yvc {

Vec VectorEngine::vectorize_patch(const ImagePatch& patch) {
    Vec v(patch.pixels.size());
    for (size_t i = 0; i < patch.pixels.size(); i++)
        v[i] = patch.pixels[i] / 255.0f;
    return v;
}

VecList VectorEngine::vectorize_patches(const std::vector<ImagePatch>& patches) {
    VecList vecs;
    vecs.reserve(patches.size());
    for (const auto& p : patches)
        vecs.push_back(vectorize_patch(p));
    return vecs;
}

ImagePatch VectorEngine::devectorize_patch(const Vec& v, int patch_w, int patch_h, int channels) {
    ImagePatch patch;
    patch.width = patch_w;
    patch.height = patch_h;
    patch.channels = channels;
    patch.pixels.resize(v.size());
    for (size_t i = 0; i < v.size(); i++) {
        float val = v[i] * 255.0f;
        val = std::max(0.0f, std::min(255.0f, val));
        patch.pixels[i] = (uint8_t)(val + 0.5f);
    }
    return patch;
}

} // namespace yvc
