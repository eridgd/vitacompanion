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
- Add `EPSV` support using the existing passive data socket setup.
- Add `NLST` support for clients that request filename-only listings.
- Add `MDTM` support for clients that query remote modification times.
- Bound control and data socket operations so disconnected clients cannot leave
  worker threads blocked indefinitely.
- Stop directory and file transfers when the peer disconnects or a partial
  socket write fails.
- Retry transient accept failures instead of permanently abandoning the FTP
  listening socket under temporary resource pressure.
- Detach clients before waiting for their threads during shutdown so network
  state changes cannot deadlock the client-list mutex.
