/*
 * Copyright (c) 2015-2016 Alexander Merritt.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Alexander Merritt may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Alexander Merritt ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL Alexander
 * Merritt BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Bit string operations intended for bit strings larger than a single
 * primitive type (e.g., many hundreds of bits).
 *
 * We assume unsigned char is 1 byte.
 *
 */

#ifndef _VM_BITOPS_H_
#define _VM_BITOPS_H_

static inline void
set_bit(unsigned char *set, int n, int bit)
{
	int nc = (bit >> 3);
	int c = (n - 1 - nc);
	int b = (bit - (nc << 3));
	set[c] |= ((1 << b) & 0xff);
}

static inline void
clear_bit(unsigned char *set, int n, int bit)
{
	int nc = (bit >> 3);
	int c = (n - 1 - nc);
	int b = (bit - (nc << 3));
	set[c] &= ~((1<<b) & 0xff);
}

static inline int
bit_isset(unsigned char *set, int n, int bit)
{
	int nc = (bit >> 3);
	int c = (n - 1 - nc);
	int b = (bit - (nc << 3));
	return !!(set[c] & ((1<<b) & 0xff));
}

/* return index of the Nth set bit */
static inline int
nth_bitset(unsigned char *set, int n, int bit)
{
	int c, nth = 0, idx = 0;
	if (bit < 1) /* no such thing as zeroth bit */
		return (-1);
	for (c = (n-1); c >= 0; c--) {
		idx = (n-1-c) << 3;
		unsigned char ch = set[c];
		/* while still bits set to discover */
		while (ch) {
			/* skip zeros */
			while (ch && !(ch & 0x1)) {
				ch = ch >> 1;
				idx++;
			}
			KKASSERT(ch); // XXX
			/* found a bit */
			nth++;
			/* done? */
			if (nth == bit)
				return (idx);
			ch = ch >> 1;
			idx++;
		}
	}
	return (-1);
}

#endif
