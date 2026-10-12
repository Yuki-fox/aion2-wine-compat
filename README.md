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

### 4. Add it to the NotProton launcher

Back up `~/Library/Application Support/Steam/compatibilitytools.d/notproton-26.3/run`, then open that file in a plain-text/code editor. Find this existing line:

```sh
export WINEDEBUG="${WINEDEBUG:-err+all,fixme-all}"
```

Immediately **below it**, add the following block once. This goes in the `run` file, **not** in Steam's Launch Options or Terminal:

```sh
# BEGIN AION 2 Rosetta compatibility fix
if [ "$app_id" = 3393110 ]; then
  export STEAM_DYLD_INSERT_LIBRARIES="$HOME/Library/Application Support/notproton/aion2-compat/compat_cpuid.dylib${STEAM_DYLD_INSERT_LIBRARIES:+:$STEAM_DYLD_INSERT_LIBRARIES}"
fi
# END AION 2 Rosetta compatibility fix
```

### 5. Check and launch

Save the file, then check it in Terminal:

```sh
sh -n "$HOME/Library/Application Support/Steam/compatibilitytools.d/notproton-26.3/run"
```

No output means the shell syntax is valid. If it reports an error, fix the edit before continuing. Launch AION 2 with **Play** in Steam as usual.

This preserves the existing overlay injection and applies only to AION 2 (3393110). Other launcher versions may handle injection differently. A NotProton update may overwrite the modification.

To undo, close the game and remove only the marked block. No game data needs deleting.

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
