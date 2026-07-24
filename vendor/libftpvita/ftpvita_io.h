#ifndef FTPVITA_IO_H
#define FTPVITA_IO_H

typedef int (*ftpvita_send_func)(int socket, const void *buffer, unsigned int length, int flags);

int ftpvita_send_all(ftpvita_send_func send_func, int socket,
	const void *buffer, unsigned int length, int flags);

#endif
