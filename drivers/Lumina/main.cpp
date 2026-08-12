#include <LOG.hpp>
#include <HAL.hpp>
#include <DRIVER.hpp>
#include <SERVICES.hpp>
#include <lib/str.hpp>
#include <stdint.h>

using namespace HAL;
using namespace MEM;

namespace Lumina {
    void input_callback(uint64_t pass) {
        (void)pass;
        // this should actually do something
    }

    int main() {
        
        for (;;);
        return 0;
    }
}

module_init(Lumina::main)