# XP Run Instructions (Minimal)

Use **only** the prebuilt binaries and config at the top level.

## Files to place on XP (same folder)
- `[ProcmonAgent.exe](ProcmonAgent.exe)`
- `[agent.ini](agent.ini)`
- `[procmon_harness.exe](procmon_harness.exe)`

## What to run

### 1) Validate device access (recommended first step)
Run:

```
procmon_harness.exe --device \\.\ProcmonDebugLogger --dump procmon-raw.bin
```

### 2) Run the agent
Run:

```
ProcmonAgent.exe
```

If you only want a single executable, **run ProcmonAgent.exe** and ignore the harness.
