# Vitacompanion

Vitacompanion is a user module which makes developing homebrews for the PS Vita device easier. It:

- Opens a FTP server on port 1337
- Listens to commands on port 1338
- Simulates controller and touch input through a small kernel module

This is a fork of [devnoname120/vitacompanion](https://github.com/devnoname120/vitacompanion)
that fixes two reliability problems in long unattended sessions: FTP transfers
failing after a few dozen files, and the command port locking up for good when a
launched application hangs. See [Changes from upstream](#changes-from-upstream).

# Build

```bash
mkdir build
cd build
cmake ..
make
```

Recent CMake releases reject the minimum version declared by the VitaSDK
toolchain files; if configuration fails with a policy error, add
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5` to the `cmake` line.

The host-side tests only need a C compiler and Python 3:

```bash
python3 -m unittest discover -s tests
```

For a side-by-side test build that does not replace the canonical
`1337`/`1338` service, configure distinct ports and a distinct module name:

```bash
cmake .. \
  -DVITACOMPANION_FTP_PORT=1340 \
  -DVITACOMPANION_CMD_PORT=1341 \
  -DVITACOMPANION_MODULE_NAME=vitacompanion_test
make
```

# Install

Run VitaShell on your PS Vita, press SELECT to activate the FTP server and
copy both `vitacompanion.suprx` and `vitacompanion_kernel.skprx` to
`ur0:/tai`. Add the kernel module under `*KERNEL` and the user module under
`*main` in `ur0:/tai/config.txt`:

```
*KERNEL
ur0:tai/vitacompanion_kernel.skprx

*main
ur0:tai/vitacompanion.suprx
```

The two modules are loaded separately by taiHEN. The kernel module is required
because the user module imports its input API; the user module does not load
the kernel module itself. Reboot after replacing either module so older copies
are not left resident in memory.

# Usage

## FTP server

You can upload stuff to your vita by running:
```
curl -q -T somefile.zip ftp://IP_TO_VITA:1337/ux0:/somedir/somefile.zip
```
Or you can use your regular FTP client. The server accepts both Vita-style
paths such as `ux0:/somedir/` and FTP absolute paths such as `/ux0:/somedir/`.
It supports passive mode with `PASV`/`EPSV`, unrestricted IPv4 active mode
with `PORT`/`EPRT`, modern and traditional directory listings, ASCII and binary
transfers, file metadata, and transfer restart/append commands for compatibility
with generic FTP clients.

If you want curl to send the full FTP path directly instead of changing
directories first, use the double-slash URL form:
```
curl -q --ftp-method nocwd ftp://IP_TO_VITA:1337//ux0:/somedir/
```

Transfer buffers and per-client state are allocated as kernel memory blocks
rather than from a user-space heap, so long sessions with many transfers do
not fragment or exhaust a pool.

### Running commands over FTP

The FTP server also accepts `SITE <command chain>`, which runs the same
commands as the command server and returns the result as a multi-line reply.
Each FTP client is served on its own thread, so this works even when the
command port is stuck behind a command that never returns:

```
printf 'SITE reboot\r\n' | nc IP_TO_VITA 1337
```

Command clients that support it can send it as `SITE version`,
`SITE nosleep status` or `SITE press cross; wait 100ms; release cross`.
Successful replies use code `200`, failed ones `500`, one response line per
`200-`/`500-` continuation line.

## Command server

Send a command by opening a TCP connection to the port 1338 of your Vita.

For example, you can reboot your vita by running:
```
echo reboot | nc IP_TO_PSVITA 1338
```

Note that you need to append a newline character to the command that you send. `echo` already adds one, which is why it works here.

Each connection is served by its own worker thread, so a command that blocks
(for example a `launch` whose target hangs while booting) does not stall other
connections. Up to four requests may be in flight at once; while all four are
busy, only a bare `reboot` is accepted so that the console can always be
recovered remotely. `SITE reboot` over the FTP port is a second recovery path.

Multiple commands can be executed sequentially by separating them with
semicolons. The final semicolon is optional:

```
echo 'press cross; wait 100ms; release cross' | nc IP_TO_PSVITA 1338
```

### Available commands

| Command   | Arguments                       | Explanation                  |
| --------- | ------------------------------- | ---------------------------- |
| `help`    | none                            | display the help screen      |
| `launch`  | `<TITLEID>`                     | launch an application by id e.g. `launch VHBB00001` to launch the [Vita Homebrew Browser](https://github.com/devnoname120/vhbb) |
| `nosleep` | `on`, `off` or `status`         | enable or disable automatic suspend prevention. This is enabled by default at boot |
| `press`   | input target and values         | press a button, position a stick, or start/update a touch |
| `quit`    | `<TITLEID>` or `all`            | quit an application by id, or all running applications |
| `reboot`  | none                            | reboot the console           |
| `release` | input target                    | release one input or all synthetic input |
| `screen`  | `on` or `off`                   | turn screen on or off        |
| `version` | none                            | display the loaded Vita Companion module version |
| `wait`    | duration ending in `ms` or `s`  | wait before executing the next chained command |

`wait` accepts integer durations such as `wait 1000ms` and `wait 3s`.

### Recovering a stuck console

`launch` hands the title to the system launcher and can block indefinitely if
the target never finishes booting (a PSP title launched outside Adrenaline's
own boot chain, for instance). The worker that ran it stays stuck, but the
port keeps answering. Recovery options, in order of preference:

1. `echo reboot | nc IP_TO_PSVITA 1338` — always accepted, even when every
   worker is busy.
2. `printf 'SITE reboot\r\n' | nc IP_TO_PSVITA 1337` — same command over the
   FTP server, which serves each client on a separate thread.
3. A physical power-hold, if the network itself is gone.

Buttons use the following form:

```
press cross
release cross
```

Supported button names are `select`, `start`, `up`, `right`, `down`, `left`,
`l`, `r`, `l1`, `r1`, `l2`, `r2`, `l3`, `r3`, `triangle`, `circle`, `cross`
(`x` is an alias), `square`, and `ps`.

Use `left-stick` or `right-stick` as the analog-stick target. Coordinates are
integers from 0 through 255, with 128 as the center:

```
press left-stick 0 128
release left-stick
press right-stick 255 128
release right-stick
```

Front and rear touches have four independently controlled slots, numbered 0
through 3. Repeating `press` for an active slot moves that same touch while
preserving its contact ID. Coordinates use the Vita's raw 1920 by 1088 touch
space:

```
press front-touch 0 960 544
release front-touch 0
press rear-touch 1 400 300
release rear-touch 1
```

Use `release all` to clear every synthetic button, stick, and touch.
 
 **Note**: Commands are defined in [`src/cmd_definitions.c`](https://github.com/robsdedude/vitacompanion/blob/master/src/cmd_definitions.c), you can add new commands there.
 
 # Integration in IDE's
 
 ## VSCode
 
 https://github.com/imcquee/vitacompanion-VSCODE
 
# Changes from upstream

Based on upstream `d467723`. Behaviour differences to be aware of when moving
scripts over:

- **FTP transfers no longer degrade over a session.** Upstream allocates
  transfer buffers from a small user-space pool that fragments and corrupts
  itself after roughly 60 transfers, after which `RETR`/`STOR` fail while
  `LIST` still works. Buffers are now kernel memory blocks. Allocation failures
  reply `451` instead of `550`.
- **The command port survives a hung command.** Each connection runs on its
  own worker thread, up to four at a time. Scripts that assumed commands were
  serialised across connections should chain them in one request instead.
  While all workers are busy, other requests get
  `Error: Too many commands in progress; only 'reboot' is accepted until one finishes.`
- **`SITE <command chain>` on the FTP port** runs command-port commands and
  is the fallback for `reboot` if the command port is ever unreachable.
- Module version reports `1.08`.

# Acknowledgements 

Thanks to xerpi for his [vita-ftploader](https://bitbucket.org/xerpi/vita-ftploader/src/87ef1d13a8aa/plugin/?at=master) plugin, I stole a lot of his code (with his permission). Thanks to cpasjuste for [PSP2SHELL](https://github.com/Cpasjuste/PSP2SHELL), it inspired me to create this tool.
