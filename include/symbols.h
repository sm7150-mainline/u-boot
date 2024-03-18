/* SPDX-License-Identifier: GPL-2.0+ */

#include <string.h>
#include <asm/types.h>

#define KSYM_NAME_LEN 127

#if IS_ENABLED(CONFIG_SYMBOL_LOOKUP)
const char * __attribute__((weak)) symbols_lookup(unsigned long addr, unsigned long *symaddr, unsigned long *offset,
			   char *namebuf);
#else
static inline const char *symbols_lookup(unsigned long addr, unsigned long *symaddr, unsigned long *offset,
			   char *namebuf)
{
	strcpy(namebuf, "???");
	namebuf[3] = '\0';
	return namebuf;
}
#endif
