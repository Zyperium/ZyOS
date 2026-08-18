#pragma once
#include <winlib/r0ui_protocol.hpp>


namespace R0UI::AutoUI {
    // Root object. Contains an x, y point.
    class Object {
    private:
        Object *next;
        Object *previous;
        
    public:
        size_t pos_x, pos_y;

        Object *GetNext();
        void Insert(Object *addr);
        void Remove();

        virtual void Draw();
    };

    class Window {
    private:
        WinControl *true_ref;
        Object *linked_queue;

        friend Object;
    public:
        Window();
        ~Window();

        // pass nullptr for things you don't want to fetch
        void GetDefs(int *x, int *y, int *width, int *height);

        // pass nullptr for things you don't want to get
        void SetDefs(int *nx, int *ny, int *nw, int *nh);

        void Update(); // run this as much as possible.
    };

    class Panel : public Object {
    private:
    public:
        void Draw() override;
    };
}