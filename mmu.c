#include "mmu.h"
#include <stddef.h>

static uint64_t pml4[512] __attribute__((aligned(4096)));
static uint64_t pml3[2][512] __attribute__((aligned(4096)));
static uint64_t pml2[2][512] __attribute__((aligned(4096)));
static uint64_t pml1[2][512] __attribute__((aligned(4096)));

__attribute__((optimize(3),always_inline)) static inline int is_in_el2(void)
{
    uint64_t currentel;
    asm volatile("mrs %0, currentel":"=r"(currentel));
    return currentel == 8;
}

static uint64_t old_mair, old_tcr, old_sctlr;

__attribute__((optimize(3),always_inline)) static inline void do_enable_mmu(uint64_t ttbr0, int el)
{
    uint64_t sctlr;
    asm volatile("mrs %0, sctlr_el%1":"=r"(sctlr):"i"(el));
    if((sctlr & 1))
    {
        sctlr &= -2;
        asm volatile("msr sctlr_el%1, %0"::"r"(sctlr),"i"(el):"memory");
    }
    old_sctlr = sctlr;
    asm volatile("mrs %0, mair_el%1":"=r"(old_mair):"i"(el));
    asm volatile("msr mair_el%1, %0"::"r"(255ull),"i"(el));
    asm volatile("mrs %0, tcr_el%1":"=r"(old_tcr):"i"(el));
    asm volatile("msr tcr_el%1, %0"::"r"(17ull),"i"(el));
    asm volatile("msr ttbr0_el%1, %0"::"r"(ttbr0),"i"(el));
    sctlr |= 0x1005;
    asm volatile("msr sctlr_el%1, %0"::"r"(sctlr),"i"(el));
}

__attribute__((optimize(3),always_inline)) static inline void do_disable_mmu(int el)
{
    asm volatile("msr sctlr_el%1, %0"::"r"(old_sctlr),"i"(el):"memory");
    asm volatile("msr tcr_el%1, %0"::"r"(old_tcr),"i"(el));
    asm volatile("msr mair_el%1, %0"::"r"(old_mair),"i"(el));
}

__attribute__((optimize(3))) void enable_mmu(uintptr_t start, uintptr_t end)
{
    start &= -4096;
    end = (end + 4095) & -4096;
    uint64_t(*pml3_alloc)[512] = pml3;
    uint64_t(*pml2_alloc)[512] = pml2;
    uint64_t(*pml1_alloc)[512] = pml1;
    while(start < end)
    {
        size_t idx = (start >> 39) & 511;
        if(!pml4[idx])
            pml4[idx] = (uintptr_t)(pml3_alloc++) | 0x623;
        uintptr_t* pml3 = (uintptr_t*)(pml4[idx] & -4096);
        idx = (start >> 30) & 511;
        if(!pml3[idx])
            pml3[idx] = (uintptr_t)(pml2_alloc++) | 0x623;
        uintptr_t* pml2 = (uintptr_t*)(pml3[idx] & -4096);
        idx = (start >> 21) & 511;
        if((start & 0x1fffff) == 0 && end - start >= 0x200000)
        {
            pml2[idx] = start | 0x621;
            start += 0x200000;
            continue;
        }
        if(!pml2[idx])
            pml2[idx] = (uintptr_t)(pml1_alloc++) | 0x623;
        uintptr_t* pml1 = (uintptr_t*)(pml2[idx] & -4096);
        idx = (start >> 12) & 511;
        pml1[idx] = start | 0x623;
        start += 4096;
    }
    if(is_in_el2())
        do_enable_mmu((uintptr_t)pml4, 2);
    else
        do_enable_mmu((uintptr_t)pml4, 1);
}

__attribute__((optimize(3))) void disable_mmu(uintptr_t start, uintptr_t end)
{
    start &= -4096;
    end = (end + 4095) & -4096;
    for(uintptr_t i = start; i < end; i += 16)
        asm volatile("dc civac, %0"::"r"(i));
    if(is_in_el2())
        do_disable_mmu(2);
    else
        do_disable_mmu(1);
}
