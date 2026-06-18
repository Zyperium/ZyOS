#pragma once
#include <stdint.h>
#include <Library/cystr.hpp>

namespace TTY::Commands {
    struct uCMD {
        uint32_t hash;
        lib::string (*invoke)(int, char **);
    };


    namespace PROC {
        lib::string echo_processor(int argc, char **argv);
        lib::string ls_processor(int argc, char **argv);
        lib::string swapd_processor(int argc, char **argv);
        lib::string cd_processor(int argc, char **argv);
        lib::string touch_processor(int argc, char **argv);
        lib::string cat_processor(int argc, char **argv);
        lib::string clear_processor(int argc, char **argv);
        lib::string driver_processor(int argc, char **argv);
        lib::string execute_processor(int argc, char **argv);
        lib::string watch_processor(int argc, char **argv);
        lib::string whatpid_processor(int argc, char **argv);
    }

    lib::string evaluate_cmd(const char *cmd);
}