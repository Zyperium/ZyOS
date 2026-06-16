#pragma once
#include <stdint.h>

namespace ELF {
    constexpr uint64_t ELF_MAGIC = 0x464C457F;

    constexpr uint64_t SHT_PROGBITS = 1;
    constexpr uint64_t SHT_SYMTAB = 2;
    constexpr uint64_t SHT_STRTAB = 3;
    constexpr uint64_t SHT_RELA = 4;
    constexpr uint64_t SHT_NOBITS = 8;
    constexpr uint64_t SHT_REL = 9;

    constexpr uint64_t SHF_WRITE = 0x1;
    constexpr uint64_t SHF_ALLOC = 0x2;
    constexpr uint64_t SHF_EXECINSTR = 0x4;

    struct Header {
        uint32_t magic;
        uint8_t  bitness;
        uint8_t  endian;
        uint8_t  header_version;
        uint8_t  abi;
        uint64_t unused;
        uint16_t type;
        uint16_t machine;
        uint32_t version;
        uint64_t entry_point;
        uint64_t ph_offset;
        uint64_t sh_offset;
        uint32_t flags;
        uint16_t header_size;
        uint16_t ph_entry_size;
        uint16_t ph_count;
        uint16_t sh_entry_size;
        uint16_t sh_count;
        uint16_t sh_str_index;
    } __attribute__((packed));

    struct ProgramHeader {
        uint32_t type;
        uint32_t flags;
        uint64_t offset;
        uint64_t vaddr;
        uint64_t paddr;
        uint64_t file_size;
        uint64_t mem_size;
        uint64_t align;
    } __attribute__((packed));

    struct SectionHeader {
        uint32_t name_offset;
        uint32_t type;
        uint64_t flags;
        uint64_t addr;
        uint64_t offset;
        uint64_t size;
        uint32_t link;
        uint32_t info;
        uint64_t addralign;
        uint64_t entsize;
    } __attribute__((packed));

    struct Symbol {
        uint32_t name;
        uint8_t  info;
        uint8_t  other;
        uint16_t shndx;
        uint64_t value;
        uint64_t size;
    } __attribute__((packed));

    struct Rela {
        uint64_t offset;
        uint64_t info;
        int64_t  addend;
    } __attribute__((packed));

    constexpr uint64_t R_X86_64_64 = 1;
    constexpr uint64_t R_X86_64_PC32 = 2;
    constexpr uint64_t R_X86_64_PLT32 = 4;
    constexpr uint64_t STB_GLOBAL = 1;
    constexpr uint64_t ELF_RELOCATABLE = 1;
}