# Haiku kernel patch: LDT and custom FS support

`0001-kernel-x86_64-Add-LDT-and-custom-FS-support-for-32-bit.patch` adds
what Wine needs to run 32-bit Windows programs (WoW64) on 64-bit Haiku:

- `THREAD_SET_LDT`: a per-team local descriptor table, set through the
  existing `"thread"` generic syscall subsystem. Only user-mode code and data
  descriptors are accepted.
- `THREAD_SET_FS_BASE`: sets the calling thread's FS base.
- For teams that used either call, the FS selector and FS base are kept
  per thread across context switches and signals (new `fs` and `fs_base`
  fields in the software area of `struct fpu_state`).
- Signal handlers always run in 64-bit mode with FS pointing at the TLS.
  Returning from a handler can resume either 64-bit or 32-bit user code.

Teams that never use the new calls behave as before.

## Applying

The patch is made against Haiku commit `2e7f922` (2026-11-05):

```sh
cd haiku
git am /path/to/0001-kernel-x86_64-Add-LDT-and-custom-FS-support-for-32-bit.patch
```

Then build and boot the kernel as usual (for example with an `@nightly-anyboot`
image, or by updating the `haiku` package in a test install). Rebuild Wine
against the patched headers: configure checks for `struct fpu_state.fs_base`
and only enables 32-bit support when it's present.

Only x86_64 is affected; the 32-bit x86 kernel is unchanged.

## License

The patch modifies Haiku source files. Like those files, it is distributed
under the MIT license:

```
Copyright (c) 2001-2026 Haiku, Inc. and contributors
Copyright (c) 2026 Techniix

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Parts of `src/system/kernel/arch/x86/64/thread.cpp` are also under the NewOS
license (Copyright 2001, Travis Geiselbrecht), as noted in that file. Each
modified file keeps its original copyright and license notices.
