#include <VFS/VFS.hpp>

#include <Library/debug.hpp>

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

    VFS::VNode *VNode::resolve_path_to_vnode(const lib::string& path) {
        if (path.length() >= 2 && path[1] == ':') {
            return nullptr;
        }

        VFS::VNode* current_node = this;
        size_t index = 0;
        lib::string token_buffer;
        if (path.length() > 0 && path[0] == '/') {
            index = 1;
        }

        while (index < path.length()) {
            token_buffer.clear();

            while (index < path.length() && path[index] != '/') {
                token_buffer += path[index];
                index++;
            }

            if (index < path.length() && path[index] == '/') {
                index++; 
            }

            if (!token_buffer.empty()) {
                VFS::VNode* next_node = current_node->lookup(token_buffer.c_str());
                Debug::krnl_print("VFS", Debug::LOG_INFO, "Lookup for '%s' returned: %x", token_buffer.c_str(), next_node);
                if (current_node != this) {
                    delete current_node; 
                    current_node = nullptr;
                }

                current_node = next_node;
                if (!current_node) {
                    return nullptr; 
                }
            }
        }

        return current_node;
    }
}