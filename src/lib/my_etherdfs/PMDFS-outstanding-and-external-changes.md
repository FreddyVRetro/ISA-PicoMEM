# PMDFS: outstanding test issues + changes outside the PicoMEM repos

Status:  DFSTEST 86/86 pass, after-reload checks 6/6 pass, shell tests
         (MOVE, DELTREE, REN *.X *.Y, single REN, COPY dates) all OK.
         Tested on MS-DOS 6.22 (486), SD card mapped to D:.
Fixes:   local branches "pmdfs-fixes" (not pushed)
           PicoMEM                 27617c9, f841594  (src/lib/my_etherdfs only)
           EtherDFS-3-for-PicoMEM  b09768b, eb211a3  (client + tests/)


## FIXED

Card firmware (DFS server, src/lib/my_etherdfs)
  [1] Rename .............. every rename reported error 1 "Invalid function"
                            though it worked (renfile() fell through to a
                            leftover POSIX rename()). Now reports success.
  [2] MOVE data loss ...... MOVE got the false error from [1] and deleted the
                            moved file. Fixed by [1]; MOVE reports [ok] and
                            the file arrives byte-identical.
  [3] Create new (5Bh) .... emptied an existing file and reported success.
                            Now honours the "create new" flag (AH=1): error
                            80, file intact. Requested RO/HID/SYS attributes
                            are applied too.
  [4] MKDIR ............... always reported success (f_mkdir result never
                            stored). Now 5 if it exists, 3 if parent missing.
  [5] RMDIR ............... could delete a FILE; wrong error codes. Now only
                            removes folders: 5 for a file or non-empty
                            folder, 3 if missing.
  [6] DELETE .............. could remove a folder; a wildcard with no match
                            reported success. Now refuses folders and
                            read-only files; no match -> error 2.
  [7] Extended open (6Ch) . wrong access mode, "replace" not implemented,
                            wrong error codes. Now open / create / replace
                            (truncate) all work; fail-if-exists -> 80;
                            missing file -> 2.
  [8] FindFirst/FindNext .. one shared iterator for all searches; no "." and
                            ".."; missing path -> 18. Now per-search state
                            (32 LRU slots), searches can run at the same
                            time, "." and ".." in subfolders (DELTREE needs
                            them), missing path -> 3.
  [9] FCB wildcard ops .... REN *.A *.B and FCB wildcard delete did nothing.
                            Now expanded on the card; delete only touches
                            normal files and skips read-only ones.
  [10] 0-byte write ....... ignored. Now truncates / extends the file at the
                            current position (DOS behaviour).
  [11] Seek from end ...... used the size cached at open time. Now uses the
                            live file size.
  [12] Error codes ........ FatFs results now mapped to the correct DOS
                            error codes everywhere.

DOS client (PMDFS.EXE)
  [13] Timestamps ......... every file was dated 01-01-2026. Close now sends
                            the date set by 5701h (COPY keeps source dates),
                            otherwise the PC's clock for written files.
  [14] 0-byte write ....... now sent to the card instead of being dropped.
  [15] Wildcard REN ....... client no longer rejects wildcards in the target
                            name.
  [16] 6Ch action code .... CX was always 0. MS-DOS leaves it to the
                            redirector, so the client now writes it into the
                            caller's register frame (1 opened / 2 created /
                            3 replaced).
  [17] Memory corruption .. every FindFirst/FindNext wrote 32 bytes ~441
                            bytes past the program's DTA (struct pointer
                            arithmetic). Now written at DTA+15h as intended.

Test suite
  tests/DFSTEST.C (86 checks + 6 after-reload checks) and tests/DFSPROBE.C
  added to the client repo; every operation checks the DOS return code AND
  the real on-disk state afterwards.


## OUTSTANDING TEST-SUITE ISSUES

Nothing fails. What's left are gaps and known limits.

Passes, but can't be fully verified
  S2  free space ...... card has >2 GB free, DOS sees a flat 2 GB, so the test
                        checks the clamp only; space dropping after a write
                        and coming back after a delete can't be seen
  F28 open files ...... stops at 14 (the 486's FILES= limit); the card's own
                        limit of 16 open files is never reached, so what
                        happens when it runs out is untested
  P6  new-file date ... compared with DOS's date only; the card stamps files
                        with the BIOS clock (4 h behind UTC here), which is
                        fine as long as DOS and the BIOS agree

Known limitations (not tested)
  Folder dates ........ new folders always 01-01-2026 (card FatFs is built
                        with a fixed clock, FF_FS_NORTC)
  Lock / unlock ....... card just reports success (upstream design); no real
                        record locking
  Commit (flush) ...... no-op on the card; data is written when the file is
                        closed

Not covered at all
  - USB drive (PMDFS U-x): same code, never run against real USB storage
  - PMDFS3.EXE (DOS 3.x client): builds, no DOS 3 machine to test on
  - FreeDOS, DOS 5 or 7 (the 6Ch action-code fix handles both possible SDA
    layouts, but only MS-DOS 6.22 was tested)
  - Full SD card, card pulled mid-operation, files > 2 GB, LFN TSRs,
    Windows 3.x on top of D:
  - Speed not measured (1 MB write + read-back takes ~1-2 min)


## CHANGES OUTSIDE THE PICOMEM REPOSITORIES

Build workarounds (none committed, only needed for the Linux build)
  PicoMEM tree ........ src/USB_Host_Driver symlink (case mismatch)
                        pm_board.h backslash includes changed to /
                        src/lib/tusb_xinput/CMakeLists.txt: "tinyusb" link
                          line commented out (submodule from a separate
                          repo, Ryzee119/tusb_xinput)
  CMake command line .. -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG
                          -Wno-error=incompatible-pointer-types"  (GCC 14)
                        picotool_DIR=/root/pico-build-r2/_deps/picotool
  Upstream DOS tools .. staging copy /root/picomem/dosbuild only:
                          SBDIAG line 202 inportp -> inportb (typo)
                          PMMOUSE asmlib case symlinks
                          LTEMM.MAC added from Lo-tech LTEMM r01
                        PMEMM still doesn't assemble (upstream source
                        unfinished)

Linux host
  UART logger ......... created /usr/local/bin/picomem-uart-logger.py and
                        picomem-uart.service (115200 ->
                        /var/log/picomem-uart.log)
                        old picocard-uart.service stopped but STILL ENABLED
                        (both will fight over /dev/ttyACM0 after a host reboot)
  COMrade MCP ......... config added for /root/picomem in ~/.claude.json, now
                        KVM target 4; the PicoPCMCIA session's COMrade bridge
                        was stopped and will need to reconnect
  Tools ............... /root/picomem/tools-src:
                          JWasm (built from source)
                          Borland C++ 3.1 subset (from v020-pure + small-model
                            libs/headers copied from the 486's C:\BCC)
                          Lo-tech LTEMM source
                          MS-DOS 4.0 source checkout (reference only)

The 486
  C:\PICOMEM .......... created: built tools, PMDFS.EXE / PMDFS3.EXE,
                        DFSTEST / DFSPROBE and their logs
                        (the two stray files my tests left there are removed)
  Unchanged ........... CONFIG.SYS / AUTOEXEC.BAT (PMDFS not loaded at boot)
                        COM1 was enabled in the BIOS by the user

Card and SD card
  Firmware ............ fixed build, flashed from the PicoMEM repo
  SD card ............. all test folders removed; the new firmware rewrote
                        PICOMEM.CFG itself (644 -> 708 bytes, new layout)

Unchanged: COMrade's code, FatFs, the Pico SDK, MS-DOS.
