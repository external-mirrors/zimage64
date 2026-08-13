# zimage64

A kernel shim for unpacking the aarch64 kernel if the bootloader does not support it.

## Building

```
make CC=aarch64-unknown-linux-gnu-gcc READELF=aarch64-unknown-linux-gnu-readelf OBJCOPY=aarch64-unknown-linux-gnu-objcopy
```

Make sure that you specify the correct cross-compiler prefix. If compiling on aarch64 you can omit them.

## Usage

```
Usage: %s [<infile> <outfile> [dtb]] [--payload <payload>...]

Compresses the aarch64 kernel image at <infile> into the self-extracting kernel image at <outfile>.
If a devicetree is specified in [dtb], it is appended after the compressed data.
If no arguments are specified, stdin/stdout are used.
One or more ARM payloads might be specified. If so, the corresponding ARM code will be run before the kernel.
```

## Payloads

Payloads should be raw ARM64 binaries, that will be run sequentially at page-aligned addresses.
In `x0`, a pointer is provided to a function that takes a path to a devicetree property in `x0` and returns the address of the property data in `x0` and its size in `x1`.

# License

```
Copyright 2025 Sonya Sireneva


This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see
<https://www.gnu.org/licenses/>.
```
