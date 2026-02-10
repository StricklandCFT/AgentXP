# Procmon Debug Logger Harness (XP)

Minimal harness to open the Procmon live driver device and dump raw stream data to disk for reverse-engineering.

## Build (VS2008 + Windows 7.1 SDK)
From a VS2008 x86 command prompt:

```
cl /EHsc /O2 /MT /DWIN32 /D_WINDOWS procmon_harness.cpp /link /SUBSYSTEM:CONSOLE,5.01
```

## Run

```
procmon_harness.exe --device \\.\ProcmonDebugLogger --dump procmon-raw.bin
```

Optional: replay a single observed IOCTL (no guessing):

```
procmon_harness.exe --device \\.\ProcmonDebugLogger --dump procmon-raw.bin --ioctl 0x222004 --inhex deadbeef --outlen 4096
```

## Notes
- If CreateFile fails, verify Procmon is running and driver is loaded.
- Do not guess IOCTLs; capture them from Procmon UI actions and replay here.
