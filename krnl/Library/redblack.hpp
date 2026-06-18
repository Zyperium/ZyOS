#pragma once
#include <stdint.h>
#include <stddef.h>
#include <Library/krnlptr.hpp>

namespace lib {
    enum class RB_Colour : bool {
        RED,
        BLACK
    };

    // inherit this!
    struct RB_Base {
        RB_Base *left{nullptr};
        RB_Base *right{nullptr};
        RB_Base *parent{nullptr};
        RB_Colour col{RB_Colour::RED};

        virtual int64_t compare(const RB_Base* other) const = 0;
        virtual ~RB_Base() = default;
    };

    class RB_Tree {
    public:
        RB_Tree() = default;

        void insert_node(RB_Base *node);
        void remove_node(RB_Base *node);

        RB_Base* get_leftmost() const { return leftmost; }
        RB_Base* get_root() const { return root; }

    protected:
        RB_Base* root{nullptr};
        RB_Base* leftmost{nullptr};

    private:
        void rotate_left(RB_Base* x);
        void rotate_right(RB_Base* y);
        void fix_insert(RB_Base* node);
        void fix_delete(RB_Base* node, RB_Base* parent);
        void transplant(RB_Base* u, RB_Base* v);
    };
}