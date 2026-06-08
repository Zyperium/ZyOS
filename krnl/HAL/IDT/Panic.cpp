#include <HAL/IDT/Panic.hpp>

#include <Library/debug.hpp>

void panic(PanicReasons reason) {
    Debug::krnl_print("IDT", Debug::LOG_INFO, "Panic!");
    (void)reason;
    
    for (;;);
}
