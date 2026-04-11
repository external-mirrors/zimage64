.section .entry
.global _start
.global image_size
.global compressed_size
.global uncompressed_size

_start:
//this disassembles to "ands x18, x0, x0" on aarch64, and to "b aarch32_trampoline" on aarch32
.long 0xea000000 + (aarch32_trampoline - _start - 8) / 4
//if we're still here, we're on aarch64. go to the actual entrypoint
b actual_start
.8byte 0 //text_offset
image_size:
.8byte 0 //image size filled in kzip.c
.8byte 2 //flags
.8byte 0 //reserved
.long 0 //reserved
//the following field is technically reserved, but fill it anyway so that 32-bit loaders can find the appended devicetree
.long _end - _start
.8byte 0 //reserved
.long 0x644d5241 //magic
.long 0
//extra headers
compressed_size:
.8byte 0
uncompressed_size:
.8byte 0

aarch32_trampoline:
//trampoline for certain MediaTek devices
//to avoid adding another cross-toolchain, the instructions are hand-assembled
.long 0xe59f0008 //ldr r0, [pc, #8]
.long 0xe24f1000 + (aarch32_trampoline - _start + 12) //sub r1, pc, #...
.long 0xe3a04001 //mov r4, #1
//arguments: r0 = 0x82000200, r1 = where to jump, r2 = devicetree (intact from the trampoline), r3 -> x1 (do not care), r4 = 1 (KERNEL_64)
.long 0xe1600070 //smc #0
.long 0x82000200 //...this is what ldr r0, ... loads

actual_start:
adr x1, stack
mov sp, x1
bl main //returns __int128
mov x30, x1
mov x1, xzr
mov x2, xzr
mov x3, xzr
ret

.section .data
.p2align 12
.long 0
.p2align 12
stack:
