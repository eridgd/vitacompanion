#ifndef FTPVITA_PROTOCOL_H
#define FTPVITA_PROTOCOL_H

#include <stddef.h>

typedef struct {
	int pending_cr;
} ftpvita_ascii_state_t;

typedef enum {
	FTPVITA_EPSV_DEFAULT,
	FTPVITA_EPSV_IPV4,
	FTPVITA_EPSV_ALL
} ftpvita_epsv_request_t;

int ftpvita_parse_port(const char *args, unsigned char address[4],
	unsigned short *port);
int ftpvita_parse_eprt(const char *args, unsigned int *protocol,
	char *address, size_t address_size, unsigned short *port);
int ftpvita_parse_epsv(const char *args, ftpvita_epsv_request_t *request,
	unsigned int *unsupported_protocol);
int ftpvita_parse_type(const char *args, char *type);
int ftpvita_command_is(const char *value, const char *expected);
int ftpvita_format_u64_decimal(char *output, size_t output_size,
	unsigned long long value);

void ftpvita_ascii_state_init(ftpvita_ascii_state_t *state);
size_t ftpvita_ascii_encode(ftpvita_ascii_state_t *state,
	const unsigned char *input, size_t input_size,
	unsigned char *output, size_t output_size);
size_t ftpvita_ascii_decode(ftpvita_ascii_state_t *state,
	const unsigned char *input, size_t input_size,
	unsigned char *output, size_t output_size);
size_t ftpvita_ascii_finish_encode(ftpvita_ascii_state_t *state,
	unsigned char *output, size_t output_size);
size_t ftpvita_ascii_finish_decode(ftpvita_ascii_state_t *state,
	unsigned char *output, size_t output_size);

#endif
