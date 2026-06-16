#include <LOG.hpp>
#include <HAL.hpp>
#include <DRIVER.hpp>
#include <stdint.h>

static int lumina_entry() {
    uint64_t *ptr = (uint64_t *)HAL::MEM::KMEM::malloc(sizeof(uint64_t));
    *ptr = 30;

    Debug::krnl_print("LUM", Debug::LOG_INFO, "Module init! Malloc'd %x with value %i", ptr, *ptr);

    HAL::MEM::KMEM::free(ptr);

    for (;;);
    return 0;
}

module_init(lumina_entry)