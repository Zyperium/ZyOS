/*
REDBLACK trees so i can implement my own CFS
*/

#include <Library/locks.hpp>
#include <Library/redblack.hpp>
#include <Library/debug.hpp>

namespace lib {
    void RB_Tree::rotate_left(RB_Base *x) {
        RB_Base *y = x->right;
        x->right = y->left;

        if (y->left != nullptr) y->left->parent = x;

        y->parent = x->parent;

        if (x->parent == nullptr) 
            root = y;
        else if (x == x->parent->left) 
            x->parent->left = y;
        else 
            x->parent->right = y;
        
        y->left = x;
        x->parent = y;
    }

    void RB_Tree::rotate_right(RB_Base *y) {
        RB_Base *x = y->left;
        y->left = x->right;

        if (x->right != nullptr) 
            x->right->parent = y;

        x->parent = y->parent;

        if (y->parent == nullptr) 
            root = x;
        else if (y == y->parent->left) 
            y->parent->left = x;
        else
            y->parent->right = x;

        x->right = y;
        y->parent = x;
    }

    void RB_Tree::insert_node(RB_Base *node) {
        ScopedLock lock(rb_lock);
        RB_Base *current = root;
        RB_Base *parent = nullptr;

        node->left = nullptr;
        node->right = nullptr;
        node->col = RB_Colour::RED;

        while (current != nullptr) {
            parent = current;
            if (node->compare(current) < 0) {
                current = current->left;
            } 
            else {
                current = current->right;
            }
        }

        node->parent = parent;

        if (parent == nullptr) {
            root = node;
        } 
        else if (node->compare(parent) < 0) {
            parent->left = node;
        } 
        else {
            parent->right = node;
        }

        if (leftmost == nullptr || node->compare(leftmost) < 0) {
            leftmost = node;
        }

        fix_insert(node);
    }

    void RB_Tree::fix_insert(RB_Base *node) {
        while (node != root && node->parent->col == RB_Colour::RED) {
            RB_Base *parent = node->parent;
            RB_Base *grandparent = parent->parent;

            if (parent == grandparent->left) {
                RB_Base *uncle = grandparent->right;

                if (uncle != nullptr && uncle->col == RB_Colour::RED) {
                    parent->col = RB_Colour::BLACK;
                    uncle->col = RB_Colour::BLACK;
                    grandparent->col = RB_Colour::RED;
                    node = grandparent;
                } 
                else {
                    if (node == parent->right) {
                        node = parent;
                        rotate_left(node);
                        parent = node->parent;
                    }

                    parent->col = RB_Colour::BLACK;
                    grandparent->col = RB_Colour::RED;
                    rotate_right(grandparent);
                }
            }
            else {
                RB_Base *uncle = grandparent->left;

                if (uncle != nullptr && uncle->col == RB_Colour::RED) {
                    parent->col = RB_Colour::BLACK;
                    uncle->col = RB_Colour::BLACK;
                    grandparent->col = RB_Colour::RED;
                    node = grandparent;
                }
                else {
                    if (node == parent->left) {
                        node = parent;
                        rotate_right(node);
                        parent = node->parent;
                    }
                    parent->col = RB_Colour::BLACK;
                    grandparent->col = RB_Colour::RED;
                    rotate_left(grandparent);
                }
            }
        }

        root->col = RB_Colour::BLACK;
    }

    static RB_Base *tree_minimum(RB_Base *node) {
        while (node->left != nullptr) {
            node = node->left;
        }
        return node;
    }

    void RB_Tree::transplant(RB_Base *u, RB_Base *v) {
        if (u->parent == nullptr) {
            root = v;
        } 
        else if (u == u->parent->left) {
            u->parent->left = v;
        } 
        else {
            u->parent->right = v;
        }
        if (v != nullptr) {
            v->parent = u->parent;
        }
    }

    void RB_Tree::remove_node(RB_Base *z) {
        if (z == nullptr) return;
        ScopedLock lock(rb_lock);

        if (z == leftmost) {
            if (z->right != nullptr) {
                leftmost = tree_minimum(z->right);
            } 
            else {
                leftmost = z->parent;
                if (leftmost == nullptr && root != z) {
                    leftmost = tree_minimum(root);
                }
            }
        }

        RB_Base *x = nullptr;
        RB_Base *y = z;
        RB_Colour y_original_color = y->col;

        if (z->left == nullptr) {
            x = z->right;
            transplant(z, z->right);
        } 
        else if (z->right == nullptr) {
            x = z->left;
            transplant(z, z->left);
        } 
        else {
            y = tree_minimum(z->right);
            y_original_color = y->col;
            x = y->right;

            if (y->parent == z) {
                if (x != nullptr) x->parent = y;
            } else {
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }

            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->col = z->col;
        }

        z->left = nullptr;
        z->right = nullptr;
        z->parent = nullptr;

        if (y_original_color == RB_Colour::BLACK && x != nullptr) {
            fix_delete(x, x->parent);
        } 
        else if (y_original_color == RB_Colour::BLACK && x == nullptr) {
            fix_delete(nullptr, y->parent);
        }
    }

    void RB_Tree::fix_delete(RB_Base* node, RB_Base* parent) {
        while (node != root && (node == nullptr || node->col == RB_Colour::BLACK)) {
            if (parent == nullptr) break;
            if (node == parent->left) {
                RB_Base* sibling = parent->right;

                if (sibling != nullptr && sibling->col == RB_Colour::RED) {
                    sibling->col = RB_Colour::BLACK;
                    parent->col = RB_Colour::RED;
                    rotate_left(parent);
                    sibling = parent->right;
                }

                if ((sibling->left == nullptr || sibling->left->col == RB_Colour::BLACK) &&
                    (sibling->right == nullptr || sibling->right->col == RB_Colour::BLACK)) {
                    sibling->col = RB_Colour::RED;
                    node = parent;
                    parent = node->parent;
                }
                else {
                    if (sibling->right == nullptr || sibling->right->col == RB_Colour::BLACK) {
                        if (sibling->left != nullptr) sibling->left->col = RB_Colour::BLACK;
                        sibling->col = RB_Colour::RED;
                        rotate_right(sibling);
                        sibling = parent->right;
                    }

                    sibling->col = parent->col;
                    parent->col = RB_Colour::BLACK;
                    if (sibling->right != nullptr) sibling->right->col = RB_Colour::BLACK;
                    rotate_left(parent);
                    node = root;
                    break;
                }
            }
            else {
                RB_Base* sibling = parent->left;

                if (sibling != nullptr && sibling->col == RB_Colour::RED) {
                    sibling->col = RB_Colour::BLACK;
                    parent->col = RB_Colour::RED;
                    rotate_right(parent);
                    sibling = parent->left;
                }

                if ((sibling->left == nullptr || sibling->left->col == RB_Colour::BLACK) &&
                    (sibling->right == nullptr || sibling->right->col == RB_Colour::BLACK)) {
                    sibling->col = RB_Colour::RED;
                    node = parent;
                    parent = node->parent;
                } 
                else {
                    if (sibling->left == nullptr || sibling->left->col == RB_Colour::BLACK) {
                        if (sibling->right != nullptr) sibling->right->col = RB_Colour::BLACK;
                        sibling->col = RB_Colour::RED;
                        rotate_left(sibling);
                        sibling = parent->left;
                    }

                    sibling->col = parent->col;
                    parent->col = RB_Colour::BLACK;
                    if (sibling->left != nullptr) sibling->left->col = RB_Colour::BLACK;
                    rotate_right(parent);
                    node = root;
                    break;
                }
            }
        }

        if (node != nullptr) {
            node->col = RB_Colour::BLACK;
        }
    }
}