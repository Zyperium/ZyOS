#include <string.h>
#include <winlib/r0ui_protocol.hpp>
#include <winlib/handler.hpp>
#include <kalloc.h>

void *operator new(size_t size) { return malloc(size); }
void *operator new[](size_t size) { return malloc(size); }
void operator delete(void *p) { free(p); }
void operator delete[](void *p) { free(p); }
void operator delete(void *p, size_t size) { free(p); (void)size; };
void operator delete[](void *p, size_t size) { free(p); (void)size; };

namespace R0UI::AutoUI {
    // This window implementation is just a basic
    // abstraction from the raw R0 protocol.
    Window::Window(const char *class_name) {
        true_ref = (WinControl *)r0ui_call(
            R0UICall::OpenWindow, 
            (uint64_t)class_name
        );

        true_ref->flags |= WindowFlags::HasDecor;
        r0ui_call(R0UICall::PushRef, 0);
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

    void Window::Update() {
        while (__atomic_test_and_set(&_lock, __ATOMIC_ACQUIRE));

        Object *root = linked_queue;

        do {
            root->Draw();
            root = root->next;
        } while (root != linked_queue);

        __atomic_clear(&_lock, __ATOMIC_RELEASE);
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

    Dim Object::FetchBounds() {
        return {0, 0, refwin->true_ref->width, refwin->true_ref->height};
    }

    void Object::Draw() {
        usr_pix_buf = refwin->true_ref->usr_pix_buf;
        return; // Base object doesn't draw anything
    }

    Panel::Panel(Window *ref, Dim &dimensions) : Object(ref, dimensions) {
        width = dimensions.width;
        height = dimensions.height;
    }

    Dim Panel::CorrectedBounds() {
        Dim resp;
        Dim restrictd = FetchBounds();

        resp.x = (pos_x > restrictd.width) ? restrictd.width : pos_x;
        resp.y = (pos_y > restrictd.height) ? restrictd.height : pos_y;

        size_t panel_right = pos_x + width;
        size_t panel_bottom = pos_y + height;
        
        if (panel_right < pos_x) panel_right = restrictd.width;
        if (panel_bottom < pos_y) panel_bottom = restrictd.height;

        if (panel_right > restrictd.width) panel_right = restrictd.width;
        if (panel_bottom > restrictd.height) panel_bottom = restrictd.height;

        resp.width = (panel_right > resp.x) ? (panel_right - resp.x) : 0;
        resp.height = (panel_bottom > resp.y) ? (panel_bottom - resp.y) : 0;

        return resp;
    }


    void Panel::Draw() {
        Object::Draw();
        
        Dim drawn = CorrectedBounds(); 
        Dim win_bounds = FetchBounds();
        
        if (drawn.width == 0 || drawn.height == 0) {
            return;
        }

        for (size_t i = 0; i < drawn.height; ++i) {
            size_t buffer_index = drawn.x + ((drawn.y + i) * win_bounds.width);
            memset32(&usr_pix_buf[buffer_index], 0xFFFFFFFF, drawn.width);
        }
    }
}