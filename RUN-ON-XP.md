# XP Run Instructions (Minimal)

Use **only** the prebuilt binaries and config at the top level.

## Files to place on XP (same folder)
- `[ProcmonAgent.exe](ProcmonAgent.exe)`
- `[agent.ini](agent.ini)`
- `[procmon_harness.exe](procmon_harness.exe)`

## What to run

### 1) Validate device access (recommended first step)
Run (auto-launches Procmon and detects the device). If Procmon is in `C:\Documents and Settings\trevor\Desktop\SysinternalsSuite`, pass that path:

```
procmon_harness.exe --auto --procmon-path "C:\Documents and Settings\trevor\Desktop\SysinternalsSuite\procmon.exe" --dump procmon-raw.bin

### 1b) If ReadFile fails, replay IOCTLs from a file
If you capture IOCTLs from Procmon into `ioctls.bin`, run:

```
procmon_harness.exe --auto --procmon-path "C:\Documents and Settings\trevor\Desktop\SysinternalsSuite\procmon.exe" --ioctl-file ioctls.bin --dump procmon-raw.bin
```
```

### 2) Run the agent
Run:

```
ProcmonAgent.exe
```

If you only want a single executable, **run ProcmonAgent.exe** and ignore the harness.
