# Vendored libftpvita

This directory vendors `xerpi/libftpvita` so vitacompanion can carry local FTP
compatibility fixes without depending on the VitaSDK-installed archive.

Upstream: https://github.com/xerpi/libftpvita
Vendored commit: `77a1d39c1d6f11a55fe65a6ed9fe707bff5ede4b`

Local changes:

- Normalize FTP command paths consistently so `ux0:/...`, `uma0:/...`, and
  `/ux0:/...` resolve to the same internal full-path form.
- Make `LIST ux0:/...` behave like `LIST /ux0:/...` instead of falling back to
  the current directory.
- Tolerate common `LIST` options such as `-a` and `-la`.
- Add `PASV`/`EPSV` lifecycle fixes, including `EPSV ALL`, safe replacement of
  an existing passive listener, and truthful protocol replies.
- Keep unrestricted IPv4 active mode through `PORT`, add IPv4 `EPRT`, reject
  malformed endpoints safely, and close a previously prepared data socket when
  the client replaces it.
- Add `NLST`, `MLST`, and `MLSD` support for modern directory clients.
- Add `MDTM` support for clients that query remote modification times.
- Support `TYPE A` NVT-ASCII conversion as well as binary `TYPE I`, plus common
  `STRU F`, `MODE S`, `REST`, `APPE`, `ABOR`, `ALLO`, `HELP`, `STAT`, and
  UTF-8/MLST option negotiation.
- Buffer the control stream so fragmented, combined, lowercase, CRLF, and
  LF-only commands are parsed without reading past command boundaries.
- Bound concurrent clients and reject overlong or NUL-containing control lines.
- Bound control and data socket operations so disconnected clients cannot leave
  worker threads blocked indefinitely.
- Stop directory and file transfers when the peer disconnects or a partial
  socket write fails.
- Retry partial file writes and make transfer restart offsets exact.
- Retry transient accept failures instead of permanently abandoning the FTP
  listening socket under temporary resource pressure.
- Synchronize listener startup and report bind/listen/thread failures to the
  caller instead of advertising an unavailable service.
- Detach clients before waiting for their threads during shutdown so network
  state changes cannot deadlock the client-list mutex.
- Allocate transfer buffers, client state, and the net init pool as SceSysmem
  memory blocks (`ftpvita_mem.c`) instead of through a user-space heap, so
  the server no longer depends on taipool. taipool's first-fit pool splits an
  exact-fit block into a header with an underflowed size, which corrupts the
  next block and leaves every later alloc/free spinning under its semaphore;
  it also cannot place a second 512 KiB buffer once any other allocation
  lands mid-pool. Both showed up as RETR/STOR failing while LIST kept working.
- Report allocation failures as transient `451` replies rather than `550`.
