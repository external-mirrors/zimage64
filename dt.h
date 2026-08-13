#pragma once
#include <stddef.h>

void maybe_replace_cmdline(void* dt);
void* resolve_property(void* dt, const char* name, size_t* size);
