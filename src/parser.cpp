#include "yvc/parser.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include <stdexcept>

namespace yvc {

// ── Image Parser ──
ImageData ImageParser::load(const std::string& path) {
    int w, h, c;
    uint8_t* raw = stbi_load(path.c_str(), &w, &h, &c, 3); // force RGB
    if (!raw)
        throw std::runtime_error("Failed to load image: " + path + " (" + stbi_failure_reason() + ")");

    ImageData img;
    img.width = w;
    img.height = h;
    img.channels = 3;
    img.pixels.assign(raw, raw + (size_t)w * h * 3);
    stbi_image_free(raw);
    return img;
}

bool ImageParser::save(const std::string& path, const ImageData& img) {
    std::string ext = path.substr(path.rfind('.'));
    for (auto& c : ext) c = (char)std::tolower(c);

    int ok = 0;
    if (ext == ".png")
        ok = stbi_write_png(path.c_str(), img.width, img.height, img.channels,
                            img.pixels.data(), img.width * img.channels);
    else if (ext == ".jpg" || ext == ".jpeg")
        ok = stbi_write_jpg(path.c_str(), img.width, img.height, img.channels,
                            img.pixels.data(), 95);
    else if (ext == ".bmp")
        ok = stbi_write_bmp(path.c_str(), img.width, img.height, img.channels,
                            img.pixels.data());
    else // default PNG
        ok = stbi_write_png(path.c_str(), img.width, img.height, img.channels,
                            img.pixels.data(), img.width * img.channels);
    return ok != 0;
}

} // namespace yvc
