#include <HAL/ACPI/ACPI.hpp>
#include <HAL/HAL.hpp>
#include <HAL/IDT/IDT.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/MEM/MEM.hpp>
#include <HAL/MEM/KMEM.hpp>
#include <HAL/CORE/Core.hpp>
#include <HAL/SCREEN/Screen.hpp>
#include <HAL/PCI/PCI.hpp>
#include <HAL/GDT/GDT.hpp>

#include <Library/debug.hpp>
#include <Library/string.h>
#include <Library/regs.h>
#include <Library/io.hpp>

#include <Services/TTY/BootTTY.hpp>

#include <limine.h>


static constinit volatile limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

static constinit volatile struct limine_hhdm_request hhdm_request {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

static constinit volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};


namespace HAL {
    void initialize() {
        char help[] = "[KRNL] <CRITICAL> HAL initialization error!";
        auto printhlp = [&]() {
            for (auto i{0uz}; i < 44; ++i)
                outb(0xE9, help[i]);
        };

        if (framebuffer_request.response == nullptr) {
            printhlp();
            for(;;);
        }

        if (hhdm_request.response == nullptr) {
            printhlp();
            for(;;);
        }

        if (memmap_request.response == nullptr) {
            printhlp();
            for(;;);
        }

        TTY::BOOT::initialize(framebuffer_request.response);

        Debug::krnl_print("HAL", Debug::LOG_INFO, "Initialize");
        MEM::initialize(&hhdm_request, memmap_request.response);

        Debug::krnl_print("HAL", Debug::LOG_INFO, "Initialize core 0");
        // This should be core 0. Otherwise something weird is going on.
        CORE::CoreLocal *data = new CORE::CoreLocal;
        data->core_id = 0;
        data->current_task = nullptr;
        data->kernel_stack = 0;
        data->self = data;
        data->lapic_ticks_per_ms = 0;

        GDT::initialize(data);

        CORE::init_core(data);

        ACPI::init();
        Debug::krnl_print("HAL", Debug::LOG_INFO, "ACPI initialized!");
        IDT::initialize();
        Debug::krnl_print("HAL", Debug::LOG_INFO, "IDT initialized!");

        SCREEN::initialize(framebuffer_request.response);

        PCI::EnumeratePCI();
    }
}