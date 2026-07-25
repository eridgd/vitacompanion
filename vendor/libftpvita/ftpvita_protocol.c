#include "ftpvita_protocol.h"

#include <limits.h>
#include <string.h>

#define FTPVITA_CONVERSION_ERROR ((size_t)-1)

static int ascii_is_space(char c)
{
	return c == ' ' || c == '\f' || c == '\n' || c == '\r' ||
		c == '\t' || c == '\v';
}

static int ascii_upper(int c)
{
	if (c >= 'a' && c <= 'z')
		return c - ('a' - 'A');
	return c;
}

static const char *skip_space(const char *value)
{
	while (value && *value && ascii_is_space(*value))
		value++;
	return value;
}

static int parse_decimal(const char **cursor, unsigned int maximum,
	unsigned int *value)
{
	const char *p = *cursor;
	unsigned int parsed = 0;
	int have_digit = 0;

	while (*p >= '0' && *p <= '9') {
		unsigned int digit = (unsigned int)(*p - '0');
		if (parsed > (maximum - digit) / 10)
			return 0;
		parsed = parsed * 10 + digit;
		have_digit = 1;
		p++;
	}

	if (!have_digit)
		return 0;

	*cursor = p;
	*value = parsed;
	return 1;
}

int ftpvita_command_is(const char *value, const char *expected)
{
	if (!value || !expected)
		return 0;

	value = skip_space(value);
	while (*value && *expected &&
		ascii_upper((unsigned char)*value) ==
		ascii_upper((unsigned char)*expected)) {
		value++;
		expected++;
	}

	if (*expected != '\0')
		return 0;

	value = skip_space(value);
	return *value == '\0';
}

int ftpvita_parse_port(const char *args, unsigned char address[4],
	unsigned short *port)
{
	const char *p;
	unsigned int fields[6];
	unsigned int i;

	if (!args || !address || !port)
		return 0;

	p = skip_space(args);
	for (i = 0; i < 6; i++) {
		if (!parse_decimal(&p, 255, &fields[i]))
			return 0;
		if (i < 5) {
			if (*p != ',')
				return 0;
			p++;
		}
	}

	p = skip_space(p);
	if (*p != '\0')
		return 0;

	for (i = 0; i < 4; i++)
		address[i] = (unsigned char)fields[i];
	*port = (unsigned short)(fields[4] * 256 + fields[5]);
	return *port != 0;
}

int ftpvita_parse_eprt(const char *args, unsigned int *protocol,
	char *address, size_t address_size, unsigned short *port)
{
	const char *p;
	const char *address_start;
	size_t address_length;
	unsigned int parsed_port;
	char delimiter;

	if (!args || !protocol || !address || address_size == 0 || !port)
		return 0;

	p = skip_space(args);
	delimiter = *p++;
	if (delimiter < 33 || delimiter > 126)
		return 0;

	if (!parse_decimal(&p, UINT_MAX, protocol) || *p++ != delimiter)
		return 0;

	address_start = p;
	while (*p && *p != delimiter)
		p++;
	if (*p != delimiter)
		return 0;
	address_length = (size_t)(p - address_start);
	if (address_length == 0 || address_length >= address_size)
		return 0;
	memcpy(address, address_start, address_length);
	address[address_length] = '\0';
	p++;

	if (!parse_decimal(&p, 65535, &parsed_port) || parsed_port == 0 ||
		*p++ != delimiter)
		return 0;

	p = skip_space(p);
	if (*p != '\0')
		return 0;

	*port = (unsigned short)parsed_port;
	return 1;
}

int ftpvita_parse_epsv(const char *args, ftpvita_epsv_request_t *request,
	unsigned int *unsupported_protocol)
{
	const char *p;
	unsigned int protocol;

	if (!args || !request || !unsupported_protocol)
		return 0;

	p = skip_space(args);
	if (*p == '\0') {
		*request = FTPVITA_EPSV_DEFAULT;
		return 1;
	}

	if (ftpvita_command_is(p, "ALL")) {
		*request = FTPVITA_EPSV_ALL;
		return 1;
	}

	if (!parse_decimal(&p, UINT_MAX, &protocol))
		return 0;
	p = skip_space(p);
	if (*p != '\0')
		return 0;

	if (protocol == 1) {
		*request = FTPVITA_EPSV_IPV4;
		return 1;
	}

	*unsupported_protocol = protocol;
	return -1;
}

int ftpvita_parse_type(const char *args, char *type)
{
	const char *p;
	char parsed;

	if (!args || !type)
		return 0;

	p = skip_space(args);
	if (*p == '\0')
		return 0;
	parsed = (char)ascii_upper((unsigned char)*p++);

	p = skip_space(p);
	if (parsed == 'A') {
		if (*p != '\0' && !ftpvita_command_is(p, "N"))
			return 0;
	} else if (parsed == 'I') {
		if (*p != '\0')
			return 0;
	} else {
		return 0;
	}

	*type = parsed;
	return 1;
}

int ftpvita_format_u64_decimal(char *output, size_t output_size,
	unsigned long long value)
{
	char reversed[20];
	size_t digits = 0;
	size_t i;

	if (!output || output_size == 0)
		return 0;

	do {
		reversed[digits++] = (char)('0' + value % 10);
		value /= 10;
	} while (value != 0);

	if (digits + 1 > output_size) {
		output[0] = '\0';
		return 0;
	}

	for (i = 0; i < digits; i++)
		output[i] = reversed[digits - i - 1];
	output[digits] = '\0';
	return 1;
}

void ftpvita_ascii_state_init(ftpvita_ascii_state_t *state)
{
	if (state)
		state->pending_cr = 0;
}

static int append_byte(unsigned char value, unsigned char *output,
	size_t output_size, size_t *used)
{
	if (*used >= output_size)
		return 0;
	output[(*used)++] = value;
	return 1;
}

size_t ftpvita_ascii_encode(ftpvita_ascii_state_t *state,
	const unsigned char *input, size_t input_size,
	unsigned char *output, size_t output_size)
{
	size_t i;
	size_t used = 0;

	if (!state || (!input && input_size) || !output)
		return FTPVITA_CONVERSION_ERROR;

	for (i = 0; i < input_size; i++) {
		unsigned char value = input[i];

		if (state->pending_cr) {
			if (value == '\n') {
				if (!append_byte('\r', output, output_size, &used) ||
					!append_byte('\n', output, output_size, &used))
					return FTPVITA_CONVERSION_ERROR;
				state->pending_cr = 0;
				continue;
			}
			if (!append_byte('\r', output, output_size, &used) ||
				!append_byte('\0', output, output_size, &used))
				return FTPVITA_CONVERSION_ERROR;
			state->pending_cr = 0;
		}

		if (value == '\r') {
			state->pending_cr = 1;
		} else if (value == '\n') {
			if (!append_byte('\r', output, output_size, &used) ||
				!append_byte('\n', output, output_size, &used))
				return FTPVITA_CONVERSION_ERROR;
		} else if (!append_byte(value, output, output_size, &used)) {
			return FTPVITA_CONVERSION_ERROR;
		}
	}

	return used;
}

size_t ftpvita_ascii_decode(ftpvita_ascii_state_t *state,
	const unsigned char *input, size_t input_size,
	unsigned char *output, size_t output_size)
{
	size_t i;
	size_t used = 0;

	if (!state || (!input && input_size) || !output)
		return FTPVITA_CONVERSION_ERROR;

	for (i = 0; i < input_size; i++) {
		unsigned char value = input[i];

		if (state->pending_cr) {
			if (value == '\n') {
				if (!append_byte('\n', output, output_size, &used))
					return FTPVITA_CONVERSION_ERROR;
				state->pending_cr = 0;
				continue;
			}
			if (value == '\0') {
				if (!append_byte('\r', output, output_size, &used))
					return FTPVITA_CONVERSION_ERROR;
				state->pending_cr = 0;
				continue;
			}
			if (!append_byte('\r', output, output_size, &used))
				return FTPVITA_CONVERSION_ERROR;
			state->pending_cr = 0;
		}

		if (value == '\r')
			state->pending_cr = 1;
		else if (!append_byte(value, output, output_size, &used))
			return FTPVITA_CONVERSION_ERROR;
	}

	return used;
}

size_t ftpvita_ascii_finish_encode(ftpvita_ascii_state_t *state,
	unsigned char *output, size_t output_size)
{
	if (!state || !output)
		return FTPVITA_CONVERSION_ERROR;
	if (!state->pending_cr)
		return 0;
	if (output_size < 2)
		return FTPVITA_CONVERSION_ERROR;
	output[0] = '\r';
	output[1] = '\0';
	state->pending_cr = 0;
	return 2;
}

size_t ftpvita_ascii_finish_decode(ftpvita_ascii_state_t *state,
	unsigned char *output, size_t output_size)
{
	if (!state || !output)
		return FTPVITA_CONVERSION_ERROR;
	if (!state->pending_cr)
		return 0;
	if (output_size < 1)
		return FTPVITA_CONVERSION_ERROR;
	output[0] = '\r';
	state->pending_cr = 0;
	return 1;
}
