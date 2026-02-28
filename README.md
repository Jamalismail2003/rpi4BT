# rpi4BT

Port of the [Circle](https://github.com/phorton1/circle-prh/tree/master/bt) bare‑metal Bluetooth Classic stack to QNX 7 for the Raspberry Pi 4.  
The project replaces the original Circle hardware glue with QNX drivers and resource-manager plumbing so QNX applications can pair with a controller (e.g., PlayStation), exchange SDP data, and pass HID/RFCOMM traffic over the on-board BCM4345C0 radio.

## Features
- **Full Bluetooth Classic path** – custom PL011 UART transport, HCI, RFCOMM, SDP parser, and HID profile layers adapted for QNX/aarch64.
- **Resource manager interface** – exposes `/dev/bt_device` so user processes can open the stack, select a remote peer, and exchange RFCOMM frames via `read/write/devctl`.
- **Interactive test console** – `menu.c` CLI to scan (`i`), list (`l`), pair (`p`), run SDP queries (`s`), open RFCOMM/HID channels (`R`, `H`), send test payloads (`o`), and disconnect (`c`).
- **Client callback queue** – `client.c` queues inbound RFCOMM payloads and wakes waiting readers, making it easy to bridge data to higher-level services (CarPlay, HID consumers, etc.).
- **Runs entirely on the RPi4** – touches GPIO, AUX, UART, and interrupt controller registers directly; no Linux/BSD shim layer required.

## Repository Layout
| Path | Description |
| --- | --- |
| `rpi4bt.c`, `menu.c` | Entry point and interactive console. |
| `transport*.c`, `hci*.c` | UART transport + HCI state machine and event parsing. |
| `rfcomm.c`, `l2cap.c`, `hid.c` | RFCOMM channels, L2CAP helpers, and HID profile support. |
| `sdp.c`, `sdp_parser.c`, `sdp_defs.h` | SDP request builder and parser. |
| `client.c`, `btQueue.*`, `resmgr.c` | QNX resource manager endpoint (`/dev/bt_device`) and RFCOMM client queue helpers. |
| `public/rpi4bt/rpi4bt_msg.h` | Public devctl/shared message header installed to `/usr/include/rpi4bt/`. |
| `test/` | GoogleTest unit test development area. |
| `build/` | Build output directory created by `make` (`build/bin`, `build/obj`). |

## Prerequisites
- QNX Software Development Platform 8.0 (or later) with the aarch64le toolchain.
- Raspberry Pi 4 running QNX with access to the on-board Bluetooth controller (BCM4345C0).
- Ability to export the QNX environment (`QNX_HOST`, `QNX_TARGET`, and `PATH`) or source `qnxsdp-env.sh`.
- Optional: `gpio-bcm2711`, `mbox-bcm2711`, or `devc-serpl011` utilities to double-check clock and pin settings as described in `Todo.txt`.

## Build
1. Prepare the QNX cross-build environment (one-time per shell):
   ```bash
   source /path/to/qnxsdp-env.sh
   ```
2. Build the fixed aarch64 target:
   ```bash
   cd rpi4BT
   make
   ```
3. Output locations:
   - Binary: `build/bin/rpi4BT`
   - Objects/deps: `build/obj/`

4. Install binary and public header:
   ```bash
   make install
   ```
   This installs:
   - `/usr/bin/rpi4BT`
   - `/usr/include/rpi4bt/rpi4bt_msg.h`

5. Unit tests:
   ```bash
   make test      # build tests in test/
   make test-run  # execute run_rpi4bt_tests
   ```

## Deploy & Run on the Pi
1. Copy the binary to the target (SCP/NFS/QNX `cp`), e.g.:
   ```bash
   scp build/bin/rpi4BT root@<pi-ip>:/usr/bin/
   ```
2. On the Pi (QNX shell):
   - Ensure GPIO pins 32/33 are in ALT3 (UART3) and pins 14/15 are inputs if UART0 is unused (`gpio-bcm2711 set 32 a3`, etc.).
   - Optional: start `devc-serpl011 -b115200 -c48000000 -e -F -u3 0xfe201600,153` if you want QNX’s serial service for debugging; `rpi4BT` also programs the UART directly.
   - Confirm firmware/clock via `mbox-bcm2711 clockrate=...` if you need a non-default UART clock.
3. Run the stack as root (needed for `procmgr_ability`, mmap, and interrupt attaches):
   ```bash
   slog2info -c rpi4BT   # optional: clean log channel
   /usr/bin/rpi4BT
   ```
4. Use the console menu to drive the stack:
   - `i` = start inquiry (default 12 s)
   - `l` = list discovered/paired devices
   - `d` = select device index
   - `p` / `u` = pair / unpair
   - `s` = SDP search
   - `R` / `o` / `c` = open RFCOMM, send sample payload, close
   - `H` = connect HID (PS controller)
   - `h` = show help

The `setup_resource_manager` thread also registers `/dev/bt_device`, allowing headless clients to interact without the console.

## Resource Manager Usage
Applications can access Bluetooth services via the QNX resource manager that `rpi4BT` exposes.

```c
#include <devctl.h>
#define MY_CMD_CODE        10001
#define MY_DEVCTL_SET_MAC  __DIOTF(_DCMD_MISC, MY_CMD_CODE, char)

int fd = open("/dev/bt_device", O_RDWR);
const char mac[] = "00:1A:7D:DA:71:0A"; // remote device

// Select which peer to connect to:
devctl(fd, MY_DEVCTL_SET_MAC, mac, sizeof(mac));

// Send RFCOMM payload (chunks >127 B are split in client.c):
write(fd, payload, payload_len);

// Blocking read returns when rfCallBackStub queues inbound data:
uint8_t buf[256];
ssize_t n = read(fd, buf, sizeof(buf));   // returns -1/EAGAIN if no data
```

Key behaviors:
- `bt_client_open()` (triggered by the `devctl`) finds the desired device, selects it in `menu.c`, and opens RFCOMM channel 0x1 (iPhone) or others as configured.
- `write()` fan-outs the payload to every open RFCOMM channel (`rfLayer_sendData`), chunking to 127 bytes to respect MTU.
- `read()` blocks on an internal condition variable until `rfCallBackStub()` queues data. Use non-blocking I/O if you need polling semantics.
- `bt_client_close()` is currently a stub—issue `c` in the console to drop RFCOMM links.

## Troubleshooting
- **Stuck after reboot** – power-cycle the Pi to guarantee peripheral registers return to default (not all reset paths clear PL011/GPIO state).
- **No UART interrupts** – verify the interrupt number (153 for UART3) is free and that `procmgr_ability` succeeds; `slog2info` will show failures.
- **Slow pairing** – check the TODO notes about using the mailbox (`mbox_call`) to set the UART clock instead of the 48 MHz default.
- **Multiple clients** – extend `resmgr.c` to track per-client state if more than one application should share `/dev/bt_device`; today the global queue assumes a single writer/reader.

## Credits
Built on top of the Circle Bluetooth stack by Paul Horton and the QNX RTOS infrastructure. See `Todo.txt` and `notes.txt` for historical context and outstanding ideas.
