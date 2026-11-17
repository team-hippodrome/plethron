# OSMesg is 8 bytes on the host: message buffers and osRecvMesg destinations sized for 4-byte messages overflow

- **Category:** abi-64bit
- **Zero status:** already ported (identical port layer: `_Generic` osRecvMesg and port-owned queue storage)
- **stadion:** no PR (Stage 3, second ares round; before PR #1, history squashed into `be110fe`)
- **stadion files:** `src/port/sdk/ultra64.h`, `src/port/os_thread.cpp`

## Symptom
Crashes and garbage in the Lab / Lab PC and at boot: `Ui_ReceiveMessage` wrote 8 bytes into s32 locals; Lab UI, scheduler-client and VI queues had buffers sized for 4-byte messages and overflowed into neighbours.

## Root cause
The game keeps the SDK's `typedef void* OSMesg` (4 bytes on the N64, 8 on the host). Game code declares `OSMesg buf[N]` arrays computed for 4-byte slots or passes `s32*`/`u32*` as the receive destination.

## Fix in stadion
Message-queue storage is owned by the port (keyed by queue), not the game's buffer; `osRecvMesg` writes exactly `sizeof(*msg)` bytes for typed destinations:
```c
port_osRecvMesgSz((mq), (msg), (flag), _Generic((msg), void*: 8, default: (int)sizeof(*(msg))))
```
Integer messages go through `OS_MESG_32` / `OS_MESG_GET_32`.

## Zero / plethron
Already ported: `src/port/sdk/ultra64.h:73` (the `_Generic` macro above); `src/port/os_thread.cpp:322` ("Own the storage here (keyed by queue ...)").

## How to detect
`tools/mesg_scan.sh` (in plethron's tools) / grep `osRecvMesg(&\w+, \(OSMesg\*\)&` and `OSMesg \w+\[` in game code; ASan on stack-buffer overflows next to a receive.
