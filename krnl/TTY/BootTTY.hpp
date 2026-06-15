#pragma once
#include <limine.h>

namespace TTY::BOOT {
    void initialize(limine_framebuffer_response *response);
    void disable();
    bool is_active();
    void show_log(const char *lclass, const char *level, const char *log);
}