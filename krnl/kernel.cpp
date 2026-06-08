#include <stdint.h>
#include <limine.h>

#include <HAL/HAL.hpp>
#include <HAL/SCREEN/Screen.hpp>

#include <Scheduler/Scheduler.hpp>

#include <Library/debug.hpp>
unsigned long __stack_chk_guard = 0xDEADDEADDEADDEAD;

extern "C" void __stack_chk_fail() {

}

using namespace HAL;

extern "C" void krnlmain() {
    initialize();

    Scheduler::Initialize();

    SCREEN::fill_screen(SCREEN::COL::GREEN);
    SCREEN::flip_buffer();

    for (;;) {
        asm volatile("hlt");
    }
}