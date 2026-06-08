#include <VFS/VFS.hpp>

namespace VFS {
    void VNode::AddRef() {
        ++m_ref_count;
        return;
    }

    void VNode::Release() {
        if (--m_ref_count == 0) delete this;
    }

    FileType VNode::GetType() const {
        return m_type;
    }

    uint64_t VNode::GetSize() const { 
        return m_size; 
    }

    VNode::VNode(FileType type, uint64_t size) : m_type(type), m_size(size) {}
}