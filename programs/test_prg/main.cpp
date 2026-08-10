extern "C" void tester_func();
extern "C" void fork_test();

int main() {
    // loaded! Yay. Can't really do anything though.

    fork_test();

    return 0;
}


extern "C" int _start() {
    main();

    for (;;);
    return 0;
}