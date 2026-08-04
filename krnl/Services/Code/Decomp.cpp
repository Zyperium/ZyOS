#include <Services/Code/Decomp.hpp>
#include <Services/Code/Zydis/Zydis.h>
#include <Library/string.h>
#include <Library/debug.hpp>

#include <stdint.h>
#include <stddef.h>

namespace Decomp {
    bool capture_disasm(uint64_t rip, Disasmrep *rep, uint8_t size) {
        Debug::krnl_print("DCMP", Debug::LOG_INFO, "capture_disasm entry: rip=%x, rep=%p, size=%i", (void*)rip, rep, size);

        if (!rep) {
            Debug::krnl_print("DCMP", Debug::LOG_ERROR, "capture_disasm: 'rep' pointer is NULL!");
            return false;
        }

        if (rep->total_lines == 0 || size == 0) {
            Debug::krnl_print("DCMP", Debug::LOG_WARN, "capture_disasm invalid params: total_lines=%i, size=%i", rep->total_lines, size);
            return false;
        }

        if (!rep->lines) {
            Debug::krnl_print("DCMP", Debug::LOG_ERROR, "capture_disasm: 'rep->lines' buffer is NULL!");
            return false;
        }
        
        rep->count = 0;

        Debug::krnl_print("DCMP", Debug::LOG_INFO, "Initializing Zydis decoder and formatter...");
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

        Debug::krnl_print("DCMP", Debug::LOG_INFO, "Allocating history buffer of size %i...", size);
        DisasmLine *lines = new DisasmLine[size];
        if (!lines) {
            Debug::krnl_print("DCMP", Debug::LOG_ERROR, "capture_disasm: Failed to allocate history buffer!");
            return false;
        }

        size_t history_count = 0;
        size_t history_idx = 0;

        uint64_t sync_start = (rip > (size * 15ULL)) ? rip - (size * 15ULL) : 0;
        uint64_t curr_addr = sync_start;
        bool synchronized = false;

        Debug::krnl_print("DCMP", Debug::LOG_INFO, "Starting backward sync scan from %x to target %x", (void*)sync_start, (void*)rip);

        ZydisDecodedInstruction instr;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];

        while (curr_addr < rip) {
            if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, (void *)curr_addr, 15, &instr, operands))) {
                ZydisFormatterFormatInstruction(&formatter, &instr, operands, instr.operand_count_visible,
                                                lines[history_idx].text, LINE_SIZE, curr_addr, ZYAN_NULL);
                lines[history_idx].addr = curr_addr;

                history_idx = (history_idx + 1) % size;
                if (history_count < size) history_count++;

                curr_addr += instr.length;
            } 
            else {
                curr_addr++;
            }
        }

        if (curr_addr == rip) {
            synchronized = true;
        }

        Debug::krnl_print("DCMP", Debug::LOG_INFO, "Backward scan finished. Synchronized=%s, history_count=%i", 
                          synchronized ? "true" : "false", history_count);

        if (synchronized) {
            for (size_t i = 0; i < history_count; ++i) {
                if (rep->count >= rep->total_lines) break;

                int ring_idx = (history_idx - history_count + i + size) % size;

                rep->lines[rep->count].addr = lines[ring_idx].addr;
                strcpy(rep->lines[rep->count].text, lines[ring_idx].text);
                rep->lines[rep->count].is_rip = false;
                ++rep->count;
            }
        }

        curr_addr = rip;
        int remaining_slots = rep->total_lines - rep->count;
        Debug::krnl_print("DCMP", Debug::LOG_INFO, "Starting forward disassembly at %x for %i slots...", (void*)curr_addr, remaining_slots);

        for (int i = 0; i < remaining_slots; i++) {
            if (rep->count >= rep->total_lines) {
                Debug::krnl_print("DCMP", Debug::LOG_WARN, "rep->count overflow prevented at index %i", i);
                break;
            }

            if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, (void *)curr_addr, 15, &instr, operands))) {
                ZydisFormatterFormatInstruction(&formatter, &instr, operands, instr.operand_count_visible,
                                                rep->lines[rep->count].text, LINE_SIZE, curr_addr, ZYAN_NULL);

                rep->lines[rep->count].addr = curr_addr;
                rep->lines[rep->count].is_rip = (i == 0);
                rep->count++;

                curr_addr += instr.length;
            } 
            else {
                strcpy(rep->lines[rep->count].text, "??");
                rep->lines[rep->count].addr = curr_addr;
                rep->lines[rep->count].is_rip = (i == 0);
                rep->count++;
                ++curr_addr;
            }
        }

        Debug::krnl_print("DCMP", Debug::LOG_INFO, "Freeing temporary history buffer...");
        delete[] lines;

        Debug::krnl_print("DCMP", Debug::LOG_INFO, "capture_disasm completed successfully. Disassembled %i lines.", rep->count);
        return true;
    }

    bool capture_disasm_stack(uint64_t rip, Disasmrep *rep, DisasmLine *history_buf, uint8_t history_size) {
        if (!rep || !rep->lines || !history_buf || rep->total_lines == 0 || history_size == 0)
            return false;

        if (rip == 0 || rip == 0xFFFFFFFFFFFFFFFF) {
            Debug::krnl_print("DCMP", Debug::LOG_WARN, "Cannot disassemble invalid RIP: %x", (void *)rip);
            return false;
        }
        
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

        size_t history_count = 0;
        size_t history_idx = 0;

        uint64_t sync_start = (rip > (history_size * 15ULL)) ? rip - (history_size * 15ULL) : 0;
        uint64_t curr_addr = sync_start;
        bool synchronized = false;

        ZydisDecodedInstruction instr;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];

        while (curr_addr < rip) {
            if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, (void *)curr_addr, 15, &instr, operands))) {
                ZydisFormatterFormatInstruction(&formatter, &instr, operands, instr.operand_count_visible,
                                                history_buf[history_idx].text, LINE_SIZE, curr_addr, ZYAN_NULL);
                history_buf[history_idx].addr = curr_addr;

                history_idx = (history_idx + 1) % history_size;
                if (history_count < history_size) history_count++;

                curr_addr += instr.length;
            } 
            else {
                curr_addr++;
            }
        }

        if (curr_addr == rip) {
            synchronized = true;
        }

        if (synchronized) {
            for (size_t i = 0; i < history_count; ++i) {
                if (rep->count >= rep->total_lines) break;

                int ring_idx = (history_idx - history_count + i + history_size) % history_size;

                rep->lines[rep->count].addr = history_buf[ring_idx].addr;
                strcpy(rep->lines[rep->count].text, history_buf[ring_idx].text);
                rep->lines[rep->count].is_rip = false;
                ++rep->count;
            }
        }

        curr_addr = rip;
        int remaining_slots = rep->total_lines - rep->count;

        for (int i = 0; i < remaining_slots; i++) {
            if (rep->count >= rep->total_lines) break;

            if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, (void *)curr_addr, 15, &instr, operands))) {
                ZydisFormatterFormatInstruction(&formatter, &instr, operands, instr.operand_count_visible,
                                                rep->lines[rep->count].text, LINE_SIZE, curr_addr, ZYAN_NULL);

                rep->lines[rep->count].addr = curr_addr;
                rep->lines[rep->count].is_rip = (i == 0);
                rep->count++;

                curr_addr += instr.length;
            } 
            else {
                strcpy(rep->lines[rep->count].text, "??");
                rep->lines[rep->count].addr = curr_addr;
                rep->lines[rep->count].is_rip = (i == 0);
                rep->count++;
                ++curr_addr;
            }
        }

        return true;
    }

    Disasmrep *decomp_and_log(uint64_t at_addr, uint8_t around) {
        Debug::krnl_print("DCMP", Debug::LOG_INFO, "decomp_and_log requested for addr %x, around %i", (void*)at_addr, around);

        Disasmrep *ret = new Disasmrep();
        if (!ret) {
            Debug::krnl_print("DCMP", Debug::LOG_ERROR, "Failed to allocate Disasmrep structure!");
            return nullptr;
        }

        ret->total_lines = (around * 2) + 1;
        Debug::krnl_print("DCMP", Debug::LOG_INFO, "Allocating ret->lines array with %i items...", ret->total_lines);

        ret->lines = new DisasmLine[ret->total_lines];
        if (!ret->lines) {
            Debug::krnl_print("DCMP", Debug::LOG_ERROR, "Failed to allocate ret->lines array!");
            delete ret;
            return nullptr;
        }

        if (!capture_disasm(at_addr, ret, around)) {
            Debug::krnl_print("DCMP", Debug::LOG_WARN, "Failed to decompile rip %x around %i", (void*)at_addr, around);
            delete[] ret->lines;
            delete ret;
            return nullptr;
        }

        Debug::krnl_print("DCMP", Debug::LOG_INFO, "Decompiled rip %x around %i successfully", (void*)at_addr, around);

        return ret;
    }

    Disasmrep::~Disasmrep() {
        if (lines)
            delete[] lines;
    }
}