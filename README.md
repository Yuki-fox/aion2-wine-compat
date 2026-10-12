# AION 2 Wine/Rosetta splash-screen fix

Experimental workaround for AION 2 hanging on the small startup splash with high CPU usage on Apple Silicon.

Tested on **M3 Pro, macOS 27.0.1, CrossOver 26.3, NotProton and D3DMetal**. The game reached gameplay and passed the splash/login again on a subsequent launch. The native Steam overlay library remained loaded; purchases were not tested.

## Install

### 1. Download the library

Close AION 2. Open [Releases](https://github.com/Yuki-fox/aion2-wine-compat/releases/tag/v0.1.0), expand **Assets**, and download **`compat_cpuid.dylib`**. You do not need either of the “Source code” archives.

### 2. Put it in the right folder

Open **Terminal**. If the file is in your Downloads folder, paste:

```sh
mkdir -p "$HOME/Library/Application Support/notproton/aion2-compat"
cp "$HOME/Downloads/compat_cpuid.dylib" "$HOME/Library/Application Support/notproton/aion2-compat/compat_cpuid.dylib"
```

If you saved it elsewhere, change the first path in the `cp` command. The installed filename must be exactly `compat_cpuid.dylib`.

### 3. Allow macOS to load the downloaded library

The release is not notarized by Apple. macOS may quarantine the download and block it when the game starts, with a message such as “Apple could not verify…” or “developer cannot be verified.”

If you downloaded the library from this repository and choose to allow it, run this in Terminal **after copying it**:

```sh
xattr -d com.apple.quarantine "$HOME/Library/Application Support/notproton/aion2-compat/compat_cpuid.dylib"
```

This removes the download quarantine flag from **this one library only**. It does not disable Gatekeeper globally and does not need `sudo`. No output means success. **“No such xattr: com.apple.quarantine”** means the flag is already absent; continue. **“No such file”** means the copy/path in step 2 needs fixing.

The `.dylib` is loaded by Wine, not opened by double-clicking. `chmod +x` does not remove a macOS quarantine block. Apple's explanation of these warnings is [here](https://support.apple.com/en-us/102445). Alternatively, build the library locally with `make` (requires Apple Command Line Tools).

### 4. Set AION 2's Steam Launch Options

**Use Launch Options, not an edit to NotProton's `run` file.** The earlier instructions edited that file, but NotProton can regenerate it when Steam starts and silently remove the fix.

In Steam, right-click **AION 2 → Properties → General → Launch Options**. If the field is empty or contains only `CX_GRAPHICS_BACKEND=d3dmetal %command%`, replace it with this entire line:

```sh
CX_GRAPHICS_BACKEND=d3dmetal STEAM_DYLD_INSERT_LIBRARIES="$HOME/Library/Application Support/notproton/aion2-compat/compat_cpuid.dylib${STEAM_DYLD_INSERT_LIBRARIES:+:$STEAM_DYLD_INSERT_LIBRARIES}" %command%
```

Paste it into **Steam's Launch Options**, not Terminal. Keep the quotes, `$` characters and `%command%` exactly as shown. If you have other custom launch options, save them first and merge this assignment before the single `%command%` instead of discarding them.

If you followed the old instructions, close the game and remove only the AION 2 compatibility block you added to `notproton-26.3/run`, if it is still there. Keep the rest of the file. Do not replace a newer launcher with an old backup.

### 5. Launch

Click **Play** in Steam as usual. The setting belongs to AION 2 and survives regeneration of the NotProton launcher. The existing Steam overlay injection is preserved. This method was checked after fully quitting and reopening Steam with the original, unmodified launcher.

To undo, close the game and remove the `STEAM_DYLD_INSERT_LIBRARIES="..."` assignment from its Launch Options. For the line above, that leaves `CX_GRAPHICS_BACKEND=d3dmetal %command%`. No game data needs deleting.

## What it fixes

A startup trampoline attempts a 64→32-bit transition to execute `CPUID` at `0xFFFFFFFE`, then wrap the 32-bit instruction pointer to zero. In the observed Wine/Rosetta failure, the trampoline faults before saving its stack pointer. Exception recovery subsequently restores `RSP=0`, and Wine repeatedly faults while constructing an exception frame.

The library interposes Wine's native signal-handler registration, recognizes the exact trampoline, reproduces its saved registers and CPUID result, and forwards the resulting execute access violation to Wine with the expected 32-bit context. It does not patch game files or disable NCGuard. There are no timers or background collectors.

This is a proof of concept for the tested runner, not a general Wine fix. It assumes specific instruction bytes and exception-frame offsets, and dereferences addresses from the matched signal context. Updates may break these assumptions. Long-term stability and other configurations are untested. The immediate failure was identified; why it began occurring on an otherwise unchanged installation remains unknown.

## Build / test

```sh
make
make test
```

The x86_64 library is written to `build/compat_cpuid.dylib`. The focused test checks saved registers, stack-pointer handling, exception dispatch and the resulting context using a simulated Wine handler; it does not replace testing in the game.
