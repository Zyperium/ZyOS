#pragma once
#include <Library/string.h>
#include <HAL/MEM/VMM.hpp>
#include <stdint.h>

namespace ELF::USER {
    constexpr uint64_t STACK_BASE =     0x90000000;
    constexpr uint64_t STACK_SIZE =     16 * PAGE_SIZE;
    constexpr uint64_t STACK_FLAGS =    HAL::MEM::VMM::PTE_WRITABLE | 
                                        HAL::MEM::VMM::PTE_PRESENT | 
                                        HAL::MEM::VMM::PTE_USER | 
                                        HAL::MEM::VMM::PTE_NX;
    constexpr uint64_t FS_BASE_OFFSET =   0x800;
    constexpr uint64_t TLS_BASE_ADDR = 0xB0000000;
}