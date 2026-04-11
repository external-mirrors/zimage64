#pragma once
#include <stdint.h>

void enable_mmu(uintptr_t start, uintptr_t end);
void disable_mmu(uintptr_t start, uintptr_t end);
