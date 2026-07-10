#include <DRIVER.hpp>
#include <SERVICES.hpp>
#include <HAL.hpp>
#include <LOG.hpp>

constexpr uint16_t MIN_CORES = 2;

namespace GCPU {
    int main() {
        size_t cc = HAL::CORE::get_core_count();

        if (cc < MIN_CORES) {
            Debug::krnl_print("GCPU", Debug::LOG_ERROR, "Not enough cores to support the GCPU architecture.");
            return 1;
        }

        Debug::krnl_print("GCPU", Debug::LOG_INFO, "CPU is supported!");

        return 0;
    }
}


module_init(GCPU::main);