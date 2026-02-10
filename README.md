# Procmon XP Streaming Agent (Skeleton)

This is a minimal C/C++ skeleton for a 32-bit Windows XP agent that will read the Procmon live driver/device stream and forward events to Logstash HTTP input.

## What is implemented
- Basic configuration file parsing from `[agent.ini](agent.ini)`.
- HTTP POST sender using WinINet (XP-safe) to Logstash HTTP input.
- Basic device read loop placeholder that attempts to read a raw stream from a configured device name.
- Raw blob dump to file for reverse-engineering.

## What is not implemented yet
- Procmon device name discovery and IOCTL handshake.
- Event blob decoding.
- Full Windows Service wrapper (install/start/stop handlers).

## Build (VS2008 + Windows 7.1 SDK)
Create a Win32 console project and add these sources from `[src](src)`.
Set subsystem to Windows 5.01, platform toolset to v90, and link against `wininet.lib`.
Ensure `[agent.ini](agent.ini)` is present in the same directory as the executable.

Recommended compiler flags:
- `/O2` for release builds
- `/MT` to avoid external runtime dependencies on XP

## Run
Edit `[agent.ini](agent.ini)` and run:

```
ProcmonAgent.exe
```

## Next steps
- Implement Procmon device stream reader in `[main.cpp](src/main.cpp)`.
- Replace the placeholder event with decoded Procmon events.
