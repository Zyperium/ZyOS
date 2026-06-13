#pragma once
#include <stdint.h>
#include <Library/cystr.hpp>

namespace TTY::Commands {
    struct uCMD {
        uint32_t hash;
        lib::string (*invoke)(int, char **);
    };

    constexpr uint32_t FNV_OFFSET = 0x811c9dc5;
    constexpr uint32_t FNV_PRIME = 0x01000193;


    namespace PROC {
        lib::string echo_processor(int argc, char **argv);
        lib::string ls_processor(int argc, char **argv);
        lib::string swapd_processor(int argc, char **argv);
    }
    
    lib::string evaluate_cmd(const char *cmd);
}