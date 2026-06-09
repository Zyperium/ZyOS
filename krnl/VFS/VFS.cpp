#include <VFS/VFS.hpp>

namespace VFS {
    void VNode::add_ref() {
        ++m_ref_count;
        return;
    }

    void VNode::release() {
        if (--m_ref_count == 0) delete this;
    }

    FileType VNode::get_type() const {
        return m_type;
    }

    uint64_t VNode::get_size() const { 
        return m_size; 
    }

    VNode::VNode(FileType type, uint64_t size) : m_type(type), m_size(size) {}
}