#include "winlib/r0ui_protocol.hpp"
#include <winlib/handler.hpp>


namespace R0UI::AutoUI {
    // This window implementation is just a basic
    // abstraction from the raw R0 protocol.
    Window::Window(const char *class_name) {
        true_ref = (WinControl *)r0ui_call(
            R0UICall::OpenWindow, 
            (uint64_t)class_name
        );
    }

    Window::Window(const char *class_name, Dim &d) : Window(class_name) {
        true_ref->x = d.x;
        true_ref->y = d.y;
        true_ref->width = d.width;
        true_ref->height = d.height;
        r0ui_call(R0UICall::PushRef, 0);
    }

    Window::~Window() {
        do {
            delete linked_queue;
        } while(linked_queue);
        // TODO! (prolly needs a r0ui call exposed)
    }

    Object::Object(Window *ref) {
        while (__atomic_test_and_set(&ref->_lock, __ATOMIC_ACQUIRE));
        refwin = ref;
        if (ref->linked_queue) {
            next = ref->linked_queue;
            previous = ref->linked_queue->previous;
            ref->linked_queue->previous->next = this;
            ref->linked_queue->previous = this;
        }
        else {
            next = this;
            previous = this;
            ref->linked_queue = this;
        }

        __atomic_clear(&ref->_lock, __ATOMIC_RELEASE);
    }

    Object::Object(Window *ref, Dim &di) : Object(ref) {
        pos_x = di.x;
        pos_y = di.y;
    }

    Object::~Object() {
        while (__atomic_test_and_set(&refwin->_lock, __ATOMIC_ACQUIRE));
        if (refwin->linked_queue == this) refwin->linked_queue = this->next;
        
        if (this == this->next) {
            refwin->linked_queue = nullptr;
            __atomic_clear(&refwin->_lock, __ATOMIC_RELEASE);
            return;
        }

        next->previous = previous;
        previous->next = next;
        previous = nullptr;
        next = nullptr;
        __atomic_clear(&refwin->_lock, __ATOMIC_RELEASE);
        return;
    }

    void Object::Draw() {
        return; // Base object doesn't draw anything
    }
}