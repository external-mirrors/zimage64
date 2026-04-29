#pragma once

void* memcpy(void* dst, const void* src, unsigned long sz);
void* memset(void* s, int c, unsigned long n);

//these functions don't actually exist. we rely on gc-sections here
int memcmp(const void* a, const void* b, unsigned long sz);
