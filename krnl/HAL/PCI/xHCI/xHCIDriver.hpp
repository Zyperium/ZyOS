#pragma once
#include <stdint.h>

namespace HAL::PCI {
    class xHCI;
    class xHCIDriver {
    public:
        virtual ~xHCIDriver() = default;
        virtual void initialize(xHCI* _ctrl, uint8_t _slot, void *endpoints, int ep_count) = 0;
        virtual void on_int(uint32_t bytes_transferred) = 0;
        virtual void start() = 0;
    };
}