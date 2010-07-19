/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)selinfo.h	8.2 (Berkeley) 1/4/94
 * $DragonFly: src/sys/sys/selinfo.h,v 1.3 2007/02/18 16:13:27 corecode Exp $
 */

#ifndef _SYS_SELINFO_H_
#define	_SYS_SELINFO_H_

#if defined(_KERNEL) || defined(_KERNEL_STRUCTURES)

#include <sys/signal.h>

#include <sys/event.h>			/* for struct klist */
#include <net/netisr.h>			/* for struct notifymsglist */

/*
 * Used to maintain information about processes that wish to be
 * notified when I/O becomes possible.
 */
struct selinfo {
	pid_t	si_pid;			/* process to be notified */
	lwpid_t	si_tid;			/* lwp within this process */
	struct	klist si_note;		/* kernel note list */
	struct	notifymsglist si_mlist;	/* list of pending predicate messages */
	short	si_flags;		/* see below */
};
#define	SI_COLL	0x0001		/* collision occurred */

#define SEL_WAITING(sel)	(sel->si_pid != 0 || (sel->si_flags & SI_COLL) != 0)

#endif

#endif /* !_SYS_SELINFO_H_ */
