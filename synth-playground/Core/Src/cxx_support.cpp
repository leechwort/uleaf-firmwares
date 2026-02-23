/* cxx_support.cpp -----------------------------------------------------------
 * Bare-metal C++ runtime stubs for Cortex-M33 without a full C++ runtime.
 *
 * Required when linking C++ code with -fno-exceptions -fno-rtti and
 * specs=nano.specs (newlib-nano).
 * ---------------------------------------------------------------------------*/

#include <stddef.h>
#include <stdlib.h>

/* --------------------------------------------------------------------------
 * Pure-virtual call guard
 * Called if a pure-virtual function is invoked before full construction —
 * should never happen in well-formed code; trap here.
 * --------------------------------------------------------------------------*/
extern "C" void __cxa_pure_virtual(void)
{
    while (1) {}
}

/* --------------------------------------------------------------------------
 * atexit stub (never called on embedded — no process exit)
 * --------------------------------------------------------------------------*/
extern "C" int __cxa_atexit(void (*)(void*), void*, void*)
{
    return 0;
}

/* --------------------------------------------------------------------------
 * operator new / delete — use malloc/free from newlib (heap_1 in FreeRTOS
 * context is irrelevant here; Gingoduino uses zero-heap fixed arrays).
 * --------------------------------------------------------------------------*/
void* operator new(size_t size)   { return malloc(size); }
void* operator new[](size_t size) { return malloc(size); }
void  operator delete(void* p)              noexcept { free(p); }
void  operator delete[](void* p)            noexcept { free(p); }
void  operator delete(void* p, size_t)      noexcept { free(p); }
void  operator delete[](void* p, size_t)    noexcept { free(p); }
