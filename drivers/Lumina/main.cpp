#include <LOG.hpp>
#include <HAL.hpp>
#include <DRIVER.hpp>
#include <TTY.hpp>
#include <SERVICES.hpp>
#include <lib/str.hpp>
#include <stdint.h>

using namespace HAL;
using namespace MEM;

namespace Lumina {
    constexpr uint64_t TTY_POACH_ID = 0;
    void input_callback(uint64_t pass) {
        (void)pass;
        // this should actually do something
    }

    int main() {
        TTY::possess_host(TTY_POACH_ID);

        TTY::hook_callback(TTY_POACH_ID, TTY::Callback::KEYBOARD_INPUT, Lumina::input_callback);

        Debug::krnl_print("LUM", Debug::LOG_INFO, "Loading ring 3 compositor");
        auto task_pid = (new Scheduler::Task([](void *v_path){
                const char *path = (const char *)v_path;

                ELF::Runway(path); // now we are good!
            },
            "lumina_compositor",
            true,
            (void *)"A:/USER/LMCPSTR.ZYX" // note I do it this way because later I want to dynamically
            // resolve the compositor path
        ))->get_pid();

        // Why keep the PID instead of the task pointer? So we don't keep a bad pointer to a crashed
        // app.
        (void)task_pid;

        for (;;);
        return 0;
    }
}

module_init(Lumina::main)