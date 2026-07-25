#include "log.h"
#include "main.h"

#include <stddef.h>

void LOG(const char *str, ...)
{
#if ENABLE_LOGGING == 1
	static SceUID logfd = -1;
	if (logfd == -1) {
		logfd = sceIoOpen("ux0:dump/vitacompanion.txt", SCE_O_APPEND |
			SCE_O_CREAT | SCE_O_WRONLY, 0666);
	}
	char buff[256];
	va_list arglist;
	va_start(arglist, str);
	int len = vsnprintf(buff, sizeof(buff), str, arglist);
	va_end(arglist);
	if (len > 0) {
		if ((size_t)len >= sizeof(buff))
			len = (int)sizeof(buff) - 1;
		sceIoWrite(logfd, buff, len);
	}
#else
	(void)str;
#endif
}
