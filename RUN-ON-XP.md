# XP Run Instructions (Minimal)

Use **only** the prebuilt binaries and config at the top level.

## Files to place on XP (same folder)
- `[ProcmonAgent.exe](ProcmonAgent.exe)`
- `[agent.ini](agent.ini)`
- `[procmon_hook.dll](procmon_hook.dll)`

## What to run

### Run the agent
Run:

```
ProcmonAgent.exe
```

The agent will launch Procmon, inject the hook DLL, capture IOCTLs into `ioctls.bin`, and then attempt to read the device stream.
