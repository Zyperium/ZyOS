#pragma once

#define module_init(init_func) extern "C" __attribute__((used)) int init_module(void) { return init_func(); }