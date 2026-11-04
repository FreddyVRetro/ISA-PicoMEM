# Read-only USB block prototype

This branch starts at upstream be7b2e6031ae93bb75733c363489aa9171a7205c.
It does not include the earlier file-service/LFN experiment. The matching ELKS
branch uses the normal block queue, buffer cache and native filesystems.
Nothing in this prototype implements long-name creation or USB writes.

Build with `-DPM_RAW_USB_RO=ON` only for the paired experimental driver.
Default OFF preserves the upstream USB/FatFs implementation. ON reserves all
USB mass storage for raw access from firmware startup: no FatFs volume 1, DOS
file-service access, or USB-backed BIOS disk images. Internal SD image access
is unchanged. Boot from an image on the internal SD, not the USB test medium.
Only the first attached USB disk, LUN 0 and 512-byte sectors are supported.
Attach before opening/mounting. No hot-removal or live replacement guarantee.

## Experimental transport (not an upstream allocation)

Base+5..7 of the existing eight-port PicoMEM I/O window are reserved only in
this build; optional audio debug use of those ports is rejected at compile time.
No BIOS shared memory, disk command register, or file-service opcode is reused.
The PPC test driver is configured for base 0x2a0. Other clients must not access
these three ports while the kernel driver is active.

Port +6 reads 0xb7. Port +5 reads IDLE=0xb0, receiving=0xb1, busy=0xb2,
complete=0xb3. To submit: write 0xa5 to +5 when idle, write exactly 32 request
bytes to +7, then write 0x5a to +5. Firmware processes on its USB/command core;
+7 then streams a 32-byte response followed by 512 data bytes. Write 0xc3 to +5
to acknowledge complete. Client reset/reuse while busy is forbidden.

All multibyte integers are little endian. Request offsets:

| Offset | Bytes | Meaning |
|---|---:|---|
| 0 | 4 | `PMB1` |
| 4 | 1 | Version 1 |
| 5 | 1 | INFO=1, CLAIM=2, READ=3, RELEASE=4 |
| 6 | 2 | Zero |
| 8 | 4 | Nonzero request sequence |
| 12 | 4 | Media generation (zero for INFO) |
| 16 | 4 | Claim token (zero for INFO/CLAIM) |
| 20 | 4 | LBA (READ only) |
| 24 | 2 | Sector count, exactly 1 for READ; otherwise zero |
| 26 | 6 | Zero |

Response: same magic/version/opcode/sequence; byte 6 is status, byte 7 is 3
(read-only + exclusive raw mode), offset 12 generation, 16 token, 20 capacity
in sectors, 24 sector size, 26 completed count, 28 VID, 30 PID. Successful READ
alone returns count=1 and 512 payload bytes. Error payloads are zeroed.
Status values are defined in `src/rawblock/protocol.h`.

CLAIM uses the nonzero request sequence as the session token. A second claim
is refused until RELEASE. Attach/detach increments generation and clears the
token. The driver checks generation before every sector completion and refuses
to adopt a replacement medium during the kernel session.

The USB backend has a two-second deadline; ELKS has a five-second deadline
and yields with interrupts enabled while waiting. Exact successful DATA and
CSW transport sizes, CSW signature/tag/status/residue, current generation and
token are required. Failure latches the raw backend until firmware restart;
a late callback cannot authorize a subsequent read or overwrite host memory.
There is no WRITE or reset command. Firmware FatFs USB disk entry points fail
closed even when the client releases its claim.

## Dependencies and limits

TinyUSB is pinned to 331c263400dd5ccf77264cab1fa6bbd9f374b44e with the small
`patches/raw-usb-tinyusb.patch` completion-evidence addition. This is separate
from the earlier USB recovery patches; no recovery or write policy is claimed.
XInput ae4f3a9fd7c57a6c8c36fdba7f7b1abc9b6a1a56 needs the existing boolean-open
compatibility wrapper for this TinyUSB version (see its dependency patch).

The paired PPC512 raw-sector check passed on REDX20 (0d7d:1600, MBR/FAT16):
six saved ranges (58 sectors, below 2 MiB) matched the Mac references, with
close/reopen between ranges. The later native ELKS FAT16 mount/remount checks
matched five existing files on both passes. The paired ELKS name-handling
candidate uses existing long names up to 26 characters and aliases for longer
names. This firmware never interprets those names. No physical USB writes,
high-LBA reads, live replacement, or removal while mounted were tested.

The earlier nine-media and DOS results belong to the superseded file-service
experiment, not this raw-block implementation. Host and emulator checks are not
evidence of physical USB timing or hotplug safety. This prototype is not a
release candidate. A hardware flash requires a verified full recovery backup.

## Review boundary and follow-up

This is a read-only design proposal, superseding the ELKS use of
[PicoMEM PR #164](https://github.com/FreddyVRetro/ISA-PicoMEM/pull/164).
I would appreciate maintainer guidance on the three provisional ports, capability
advertisement, dependency revisions and USB ownership policy before adding writes.
Default OFF selects the original firmware FatFs backend; ON is intentionally
exclusive even after RELEASE. Dynamic handoff is not implemented.

A later write proposal must define sector-write completion, device-cache flushing,
write protection, uncertain-write failures and safe release. No FAT/LFN code or
USB-model-specific exceptions belong in this transport. TinyUSB's completion
reporting and recovery work should be reviewed separately from PicoMEM policy.
The current small dependency patch is read-specific and is not an upstream-ready
generic TinyUSB API proposal.

## Reproduce host checks

After preparing dependencies, run `tests/rawblock/run.sh`. It builds temporary
sanitized host binaries for the protocol core, actual mailbox service (mocked
USB), and actual pinned TinyUSB MSC state machine. The latter covers 539 transfer
length, endpoint and result cases. These checks neither access hardware nor
write a disk. `CC` can select the host C compiler.

The RP2350 test build uses a 4096-byte core-0 stack and 2048-byte core-1 stack.
The linker map confirms that the scratch-X code ends before core-1's stack and
scratch-Y is fully reserved for core-0's stack. Individual compiler stack-usage
reports are retained; this is not a measured bound on every nested USB call.

## Build the PicoMEM 2 prototype

Install Pico SDK 2.2.0 and an Arm bare-metal toolchain as for the upstream build.
In a fresh checkout, prepare the pinned dependencies and build both modes:

```sh
./prepare-raw-deps.sh
./tests/rawblock/run.sh
cmake -S src -B build/raw-ro -DPICO_SDK_PATH="$PICO_SDK_PATH" -DPM_RAW_USB_RO=ON
cmake --build build/raw-ro --parallel
cmake -S src -B build/stock -DPICO_SDK_PATH="$PICO_SDK_PATH" -DPM_RAW_USB_RO=OFF
cmake --build build/stock --parallel
```

The source's existing board selection is PicoMEM 2 / RP2350. Other boards have
not been built or hardware-tested for this proposal. Dependency preparation
refuses to replace existing checkouts. The pinned TinyUSB and XInput setup is
provided for reproduction; it does not establish the maintainer's release pins.
