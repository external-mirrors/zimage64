#pragma once
#include "stdint.h"
#include "stddef.h"

//these functions don't actually exist. we rely on gc-sections here
void* realloc(void* buf, unsigned long newsz);
