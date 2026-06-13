#pragma once
#include <stdint.h>
#include <Library/cystr.hpp>

namespace VFS {
    enum class FileType {
        Regular,
        Directory,
        CharDevice
    };

    class VNode {
    protected:
        FileType m_type;
        uint64_t m_size;
        uint32_t m_ref_count{1};

    public:
        VNode(FileType type, uint64_t size);
        virtual ~VNode() = default;
        
        virtual int read(uint64_t offset, void* buffer, uint32_t size) = 0;
        virtual int write(uint64_t offset, const void* buffer, uint32_t size) = 0;
        virtual VNode* lookup(const char* name) = 0;

        FileType get_type() const;
        uint64_t get_size() const;
        
        void add_ref();
        void release();

        VFS::VNode* resolve_path_to_vnode(const lib::string& path);
    };

    struct FileHandle {
        VNode* vnode {nullptr};
        uint64_t current_offset {0};
        uint32_t flags {0};
        bool valid {false};
    };
}