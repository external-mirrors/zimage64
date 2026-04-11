# zimage64

A kernel shim for unpacking the aarch64 kernel if the bootloader does not support it.

## Building

```
make CC=aarch64-unknown-linux-gnu-gcc READELF=aarch64-unknown-linux-gnu-readelf OBJCOPY=aarch64-unknown-linux-gnu-objcopy
```

Make sure that you specify the correct cross-compiler prefix. If compiling on aarch64 you can omit them.

## Usage

```
Usage: mkzimage64 [<infile> <outfile> [dtb]]

Compresses the aarch64 kernel image at <infile> into the self-extracting kernel image at <outfile>.
If a devicetree is specified in [dtb], it is appended after the compressed data.
If no arguments are specified, stdin/stdout are used.
```

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
