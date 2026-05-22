#pragma once
#include "yvc/types.h"
#include <string>

namespace yvc {

// ── Image Parser ──
class ImageParser {
public:
    static ImageData load(const std::string& path);
    static bool save(const std::string& path, const ImageData& img);
};

} // namespace yvc
