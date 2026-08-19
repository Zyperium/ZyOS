#include <winlib/handler.hpp>
using namespace R0UI;

extern "C" int main() {
    AutoUI::Window *root_win = new AutoUI::Window("Task Watcher");
    AutoUI::Dim panel_di{20, 20, 20, 20};
    AutoUI::Panel *some_panel = new AutoUI::Panel(root_win, panel_di);
    (void)some_panel;
    for (;;) root_win->Update();
    return 0;
}