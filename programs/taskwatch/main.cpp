#include <winlib/r0ui_protocol.hpp>

using R0UI::Event;
using R0UI::EventType;
using R0UI::WinControl;
using R0UI::R0UICall;

extern "C" int main() {
    for (;;) asm volatile("pause");
    WinControl *taskwin = (WinControl *)r0ui_call(R0UICall::OpenWindow, (uint64_t)"Task Watcher");

    r0ui_call(R0UI::R0UICall::RedrawMyWindows, 0);

    for (;;);

    return 0;
}