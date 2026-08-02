#include <Services/Code/Decomp.hpp>
#include <Services/Code/Zydis/Zydis.h>

namespace Decomp {
    struct DisasmLine {
        uint64_t addr{0};
        bool is_rip{0};
        char text[LINE_SIZE]{0};
    };

    struct DisasmReport {
        int total_lines{0};
        DisasmLine *lines{nullptr};
        int count{0};
    };

    bool capture_disasm(uint64_t rip, DisasmReport *rep) {
        if (rep->total_lines == 0)
            return false;
        
        rep->count = 0;

        ZydisDecoder decoder;
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        ZydisFormatter formatter;
        ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

        ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_HEX_PREFIX, (ZyanUPointer)"0x");
        ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_HEX_SUFFIX, (ZyanUPointer)"");

        ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_ADDR_BASE, (ZyanUPointer)ZYDIS_NUMERIC_BASE_HEX);
        ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_DISP_BASE, (ZyanUPointer)ZYDIS_NUMERIC_BASE_HEX);
        ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_IMM_BASE,  (ZyanUPointer)ZYDIS_NUMERIC_BASE_HEX);

        ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_ADDR_SIGNEDNESS, (ZyanUPointer)ZYDIS_SIGNEDNESS_UNSIGNED);
        ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_DISP_SIGNEDNESS, (ZyanUPointer)ZYDIS_SIGNEDNESS_UNSIGNED);
        ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_IMM_SIGNEDNESS,  (ZyanUPointer)ZYDIS_SIGNEDNESS_UNSIGNED);

        ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_HEX_UPPERCASE, ZYAN_TRUE);

        return true;
    }

    void decomp_and_log(uint64_t at_addr, uint8_t around) {

    }
}