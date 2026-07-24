#include "ftpvita_io.h"

int ftpvita_send_all(ftpvita_send_func send_func, int socket,
	const void *buffer, unsigned int length, int flags)
{
	const unsigned char *bytes = buffer;
	unsigned int total = 0;

	while (total < length) {
		int sent = send_func(socket, bytes + total, length - total, flags);
		if (sent < 0)
			return sent;
		if (sent == 0)
			return -1;
		total += (unsigned int)sent;
	}

	return (int)total;
}
