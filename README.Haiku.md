# Wine for Haiku

This is a port of [Wine](https://www.winehq.org/) 11.21 to
[Haiku](https://www.haiku-os.org/) x86_64. It builds on the existing
Haiku port by X512 and the HaikuPorts contributors and adds:

- **Native Haiku display driver** (`dlls/winehaiku.drv`). Windows are drawn
  in real Haiku windows, without Wayland or X11. Wine tries it before the
  Wayland driver.
- **32-bit Windows programs (WoW64)**, when running on a Haiku kernel with the
  LDT patch in [`haiku/kernel`](haiku/kernel). Older kernels can still run
  64-bit Windows programs.
- **Network adapter information** (MAC address, link state, MTU and traffic
  counters) for programs that query their network interfaces.
- The HaikuPorts patches rebased from Wine 11.8 to 11.21, with fixes:
  - The segfault debug dump no longer waits for a key press on every fault.
  - Haiku-only changes no longer leak into Linux/BSD builds.
  - The Haiku build is fixed against 11.21.

**Status: experimental.** The new code builds and passes syntax checks
against Haiku's headers, but hasn't been run on Haiku yet. Please report what
works and what doesn't.

## Building on Haiku

Install the build dependencies listed in
[`haiku/haikuports/app-emulation/wine/wine-11.21.recipe`](haiku/haikuports/app-emulation/wine/wine-11.21.recipe),
then:

```sh
autoreconf -fi        # required: the Haiku changes to configure.ac are not in ./configure
mkdir build && cd build
LDFLAGS=-lnetwork ../configure --enable-archs=i386,x86_64 --disable-tests \
    --with-wayland --without-x
make -j$(nproc)
./wine notepad
```

The generated `configure` script is kept at the upstream version, so
`autoreconf -fi` is required.

Use `WINEDEBUG=+haikudrv` for driver logs. To force a driver, set
`HKEY_CURRENT_USER\Software\Wine\Drivers\Graphics` to `haiku` or `wayland`.

### With haikuporter

[`haiku/haikuports`](haiku/haikuports) contains a recipe and a patchset
against the Wine 11.21 release tarball. Copy `app-emulation/wine` into your
HaikuPorts tree and run `haikuporter wine-11.21`. If the checksum doesn't
match, use the one haikuporter reports.

### 32-bit programs

Build and boot a Haiku kernel with
[`haiku/kernel/0001-kernel-x86_64-Add-LDT-and-custom-FS-support-for-32-bit.patch`](haiku/kernel),
then rebuild Wine against the patched headers. `configure` enables 32-bit
support when `struct fpu_state` has an `fs_base` member. On older kernels a
32-bit program fails with a message saying that LDT support is needed.

## Known limitations

- Display driver: no cursor shapes (the default pointer is used), clipboard,
  OpenGL/Vulkan, layered window transparency, display mode changes or
  multiple monitors yet.
- Network adapters: the reported link speed is a fixed placeholder.

## Credits

- The Wine project authors (see [AUTHORS](AUTHORS)).
- X512, for the original Haiku port of Wine, the Haiku driver for Wine 7.1
  that `winehaiku.drv` is based on, and the Haiku fixes that made it possible.
- Peppersawce, Ken Mays and the other [HaikuPorts](https://github.com/haikuports/haikuports)
  contributors, for maintaining the Wine recipe and patches.
- The Haiku developers.

The changes in this repository were largely written with an AI assistant
(Claude), with the maintainer directing the work and doing the building,
testing and debugging on Haiku.

## Licensing

Wine is free software, licensed under the GNU Lesser General Public License,
version 2.1 or later. See [LICENSE](LICENSE), [COPYING.LIB](COPYING.LIB) and
[AUTHORS](AUTHORS). All changes to Wine in this repository are under the same
license.

Other files:

- [`haiku/kernel`](haiku/kernel): a patch to the Haiku kernel, under the MIT
  license like the Haiku files it modifies (see
  [`haiku/kernel/README.md`](haiku/kernel/README.md)).
- [`haiku/haikuports`](haiku/haikuports): a HaikuPorts recipe, under the MIT
  license of the HaikuPorts project (see
  [`haiku/haikuports/LICENSE.md`](haiku/haikuports/LICENSE.md)).
- `dlls/winewayland.drv/input-event-codes.h`: a copy of a Linux kernel
  header, under GPL-2.0-only WITH Linux-syscall-note, as stated in the file.

### Modifications to Wine

As required by section 2(a) of the LGPL, this is a notice that the
following files of Wine 11.21 were modified for the Haiku port, starting
2026-11-05. The commit history of this repository records each change, its
author and its date.

- `README.md`
- `configure.ac`
- `dlls/crypt32/unixlib.c`
- `dlls/nsiproxy.sys/ndis.c`
- `dlls/ntdll/unix/file.c`
- `dlls/ntdll/unix/loader.c`
- `dlls/ntdll/unix/serial.c`
- `dlls/ntdll/unix/server.c`
- `dlls/ntdll/unix/signal_x86_64.c`
- `dlls/ntdll/unix/socket.c`
- `dlls/ntdll/unix/system.c`
- `dlls/ntdll/unix/thread.c`
- `dlls/ntdll/unix/virtual.c`
- `dlls/wineoss.drv/oss.c`
- `dlls/wineoss.drv/ossmidi.c`
- `dlls/winewayland.drv/wayland_keyboard.c`
- `dlls/winewayland.drv/wayland_pointer.c`
- `dlls/winex11.drv/Makefile.in`
- `dlls/winex11.drv/bitblt.c`
- `dlls/winex11.drv/keyboard.c`
- `dlls/ws2_32/unixlib.c`
- `include/wine/server.h`
- `programs/explorer/desktop.c`
- `programs/winedbg/winedbg.c`
- `server/change.c`
- `server/fd.c`
- `server/signal.c`
- `server/sock.c`
- `tools/winegcc/winegcc.c`

New files: `dlls/winehaiku.drv/*`, `dlls/winewayland.drv/input-event-codes.h`,
this file and the `haiku` directory.
