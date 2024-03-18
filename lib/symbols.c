/*
 * symbols.c: in-kernel printing of symbolic oopses and stack traces.
 * Ported from Xen hypervisor
 *
 * Copyright 2002 Rusty Russell <rusty@rustcorp.com.au> IBM Corporation
 *
 * ChangeLog:
 *
 * (25/Aug/2004) Paulo Marques <pmarques@grupopie.com>
 *      Changed the compression method from stem compression to "table lookup"
 *      compression (see tools/symbols.c for a more complete description)
 */

#include <common.h>
#include <asm/esr.h>
#include <asm/global_data.h>
#include <asm/ptrace.h>
#include <hexdump.h>
#include <linux/compiler.h>
#include <symbols.h>

extern const uint64_t symbols_offsets[];
extern const unsigned int symbols_num_syms;
extern const u8 symbols_names[];
extern const u8 symbols_token_table[];
extern const u16 symbols_token_index[];
extern const unsigned int symbols_markers[];

#define symbols_address(n) (symbols_offsets[n] + 0UL)

DECLARE_GLOBAL_DATA_PTR;

/* expand a compressed symbol data into the resulting uncompressed string,
   given the offset to where the symbol is in the compressed stream */
static unsigned int symbols_expand_symbol(unsigned int off, char *result)
{
	int len, skipped_first = 0;
	const u8 *tptr, *data;

	/* get the compressed symbol length from the first symbol byte */
	data = &symbols_names[off];
	len = *data;
	data++;

	/* update the offset to return the offset for the next symbol on
	 * the compressed stream */
	off += len + 1;

	/* for every byte on the compressed symbol data, copy the table
	 * entry for that byte */
	while (len) {
		tptr = &symbols_token_table[symbols_token_index[*data]];
		data++;
		len--;

		while (*tptr) {
			if (skipped_first) {
				*result = *tptr;
				result++;
			} else
				skipped_first = 1;
			tptr++;
		}
	}

	*result = '\0';

	/* return to offset to the next symbol */
	return off;
}

/* find the offset on the compressed stream given and index in the
 * symbols array */
static unsigned int get_symbol_offset(unsigned long pos)
{
	const u8 *name;
	int i;

	/* use the closest marker we have. We have markers every 256 positions,
	 * so that should be close enough */
	name = &symbols_names[symbols_markers[pos >> 8]];

	/* sequentially scan all the symbols up to the point we're searching for.
	 * Every symbol is stored in a [<len>][<len> bytes of data] format, so we
	 * just need to add the len to the current pointer for every symbol we
	 * wish to skip */
	for (i = 0; i < (pos & 0xFF); i++)
		name = name + (*name) + 1;

	return name - symbols_names;
}

const char *symbols_lookup(unsigned long addr, unsigned long *symaddr, unsigned long *offset, char *namebuf)
{
	unsigned long low, high, mid;

	if (addr > gd->relocaddr)
		addr -= gd->relocaddr;

	namebuf[KSYM_NAME_LEN] = 0;
	namebuf[0] = 0;

	/* do a binary search on the sorted symbols_addresses array */
	low = 0;
	high = symbols_num_syms;

	while (high - low > 1) {
		mid = (low + high) / 2;
		if (symbols_address(mid) <= addr) {
			//printf("%lu: %#lx <= %#lx\n", mid, symbols_address(mid), addr);
			low = mid;
		} else {
			//printf("%lu: %#lx > %#lx\n", mid, symbols_address(mid), addr);
			high = mid;
		}
	}

	/* search for the first aliased symbol. Aliased symbols are
	 * symbols with the same address */
	while (low && symbols_address(low - 1) == symbols_address(low))
		--low;

	/* Grab name */
	symbols_expand_symbol(get_symbol_offset(low), namebuf);

	*symaddr = symbols_address(low);
	*offset = addr - symbols_address(low);
	return namebuf;
}
