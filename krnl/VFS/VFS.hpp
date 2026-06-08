#pragma once
#include <stdint.h>

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
        
        virtual int Read(uint64_t offset, void* buffer, uint32_t size) = 0;
        virtual int Write(uint64_t offset, const void* buffer, uint32_t size) = 0;
        virtual VNode* Lookup(const char* name) = 0;

        FileType GetType() const;
        uint64_t GetSize() const;
        
        void AddRef();
        void Release();
    };

    struct FileHandle {
        VNode* vnode {nullptr};
        uint64_t current_offset {0};
        uint32_t flags {0};
        bool valid {false};
    };
}