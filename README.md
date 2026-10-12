# AION 2 Wine/Rosetta splash-screen fix

Experimental workaround for AION 2 hanging on the small startup splash with high CPU usage on Apple Silicon.

Tested on **M3 Pro, macOS 27.0.1, CrossOver 26.3, NotProton and D3DMetal**. The game reached gameplay and passed the splash/login again on a subsequent launch. The native Steam overlay library remained loaded; purchases were not tested.

## Install

1. Close AION 2. Download `compat_cpuid.dylib` from [Releases](../../releases), or build it with `make` (requires Apple Command Line Tools).
2. Put the library in `~/Library/Application Support/notproton/aion2-compat/`.
3. Back up `~/Library/Application Support/Steam/compatibilitytools.d/notproton-26.3/run`. In that script, immediately after `export WINEDEBUG="${WINEDEBUG:-err+all,fixme-all}"`, add:

```sh
# BEGIN AION 2 Rosetta compatibility fix
if [ "$app_id" = 3393110 ]; then
  export STEAM_DYLD_INSERT_LIBRARIES="$HOME/Library/Application Support/notproton/aion2-compat/compat_cpuid.dylib${STEAM_DYLD_INSERT_LIBRARIES:+:$STEAM_DYLD_INSERT_LIBRARIES}"
fi
# END AION 2 Rosetta compatibility fix
```

Check the edited script, then launch normally from Steam:

```sh
sh -n "$HOME/Library/Application Support/Steam/compatibilitytools.d/notproton-26.3/run"
```

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
