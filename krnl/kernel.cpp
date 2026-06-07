#include <stdint.h>
#include <limine.h>

#include <HAL/HAL.hpp>

#include <Library/debug.hpp>
unsigned long __stack_chk_guard = 0xDEADDEADDEADDEAD;

extern "C" void __stack_chk_fail() {

}

extern "C" void krnlmain() {
    HAL::initialize();

    for (;;) {
        asm volatile("hlt");
    }
}