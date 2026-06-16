#pragma once
#include <Library/cystr.hpp>

namespace lib {
    constexpr uint32_t MIN_PATH_LENGTH = 3;

    struct fullpath {
        char drv;
        string path;
    };

    fullpath parse_path(string path);
}