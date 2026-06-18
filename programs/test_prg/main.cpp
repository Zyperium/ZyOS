
extern "C" void tester_func();

int main() {
    // loaded! Yay. Can't really do anything though.
    tester_func();

    return 0;
}


extern "C" int _start() {
    main();

    for (;;);
    return 0;
}