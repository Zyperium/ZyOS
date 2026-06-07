#pragma once
#include <limine.h>

namespace HAL::MEM {
    void initialize(volatile limine_hhdm_request *hhdm_req, volatile struct limine_memmap_response *memmap_resp);
}