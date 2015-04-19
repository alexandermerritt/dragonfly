/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)procfs_status.c	8.3 (Berkeley) 2/17/94
 *
 * $FreeBSD: src/sys/miscfs/procfs/procfs_map.c,v 1.24.2.1 2001/08/04 13:12:24 rwatson Exp $
 * $DragonFly: src/sys/vfs/procfs/procfs_map.c,v 1.7 2007/02/19 01:14:24 corecode Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <vfs/procfs/procfs.h>

#include <vm/vm.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>

#include <machine/limits.h>

#define MEBUFFERSIZE 256

/*
 * The map entries can *almost* be read with programs like cat.  However,
 * large maps need special programs to read.  It is not easy to implement
 * a program that can sense the required size of the buffer, and then
 * subsequently do a read with the appropriate size.  This operation cannot
 * be atomic.  The best that we can do is to allow the program to do a read
 * with an arbitrarily large buffer, and return as much as we can.  We can
 * return an error code if the buffer is too small (EFBIG), then the program
 * can try a bigger buffer.
 */
int
procfs_domap(struct proc *curp, struct lwp *lp, struct pfsnode *pfs,
	     struct uio *uio)
{
	struct proc *p = lp->lwp_proc;
	int len;
	struct vnode *vp;
	char *fullpath, *freepath;
	int error;
	vm_map_t map = &p->p_vmspace->vm_map;
	pmap_t pmap = vmspace_pmap(p->p_vmspace);
	vm_map_entry_t entry;
	char mebuffer[MEBUFFERSIZE];

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	if (uio->uio_offset != 0)
		return (0);
	
	error = 0;
	vm_map_lock_read(map);
	for (entry = map->header.next;
		((uio->uio_resid > 0) && (entry != &map->header));
		entry = entry->next) {
		vm_object_t obj, tobj, lobj;
		int ref_count, shadow_count, flags;
		vm_offset_t addr;
		vm_offset_t ostart;
		int resident, privateresident;
		char *type;

		if (entry->maptype != VM_MAPTYPE_NORMAL &&
		    entry->maptype != VM_MAPTYPE_VPAGETABLE) {
			continue;
		}

		obj = entry->object.vm_object;
		if (obj)
			vm_object_hold(obj);

		if (obj && (obj->shadow_count == 1))
			privateresident = obj->resident_page_count;
		else
			privateresident = 0;

		/*
		 * Use map->hint as a poor man's ripout detector.
		 */
		map->hint = entry;
		ostart = entry->start;

		/*
		 * Count resident pages (XXX can be horrible on 64-bit)
		 */
		resident = 0;
		addr = entry->start;
		while (addr < entry->end) {
			if (pmap_extract(pmap, addr))
				resident++;
			addr += PAGE_SIZE;
		}
		if (obj) {
			lobj = obj;
			while ((tobj = lobj->backing_object) != NULL) {
				KKASSERT(tobj != obj);
				vm_object_hold(tobj);
				if (tobj == lobj->backing_object) {
					if (lobj != obj) {
						vm_object_lock_swap();
						vm_object_drop(lobj);
					}
					lobj = tobj;
				} else {
					vm_object_drop(tobj);
				}
			}
		} else {
			lobj = NULL;
		}

		freepath = NULL;
		fullpath = "-";
		if (lobj) {
			switch(lobj->type) {
			default:
			case OBJT_DEFAULT:
				type = "default";
				vp = NULL;
				break;
			case OBJT_VNODE:
				type = "vnode";
				vp = lobj->handle;
				vref(vp);
				break;
			case OBJT_SWAP:
				type = "swap";
				vp = NULL;
				break;
			case OBJT_DEVICE:
				type = "device";
				vp = NULL;
				break;
			case OBJT_MGTDEVICE:
				type = "mgtdevice";
				vp = NULL;
				break;
			}
			
			flags = obj->flags;
			ref_count = obj->ref_count;
			shadow_count = obj->shadow_count;
			if (vp != NULL) {
				vn_fullpath(p, vp, &fullpath, &freepath, 1);
				vrele(vp);
			}
			if (lobj != obj)
				vm_object_drop(lobj);
		} else {
			type = "none";
			flags = 0;
			ref_count = 0;
			shadow_count = 0;
		}

		/*
		 * format:
		 *  start, end, res, priv res, cow, access, type, (fullpath).
		 */
		ksnprintf(mebuffer, sizeof(mebuffer),
#if LONG_BIT == 64
			  "0x%016lx 0x%016lx %d %d %p %s%s%s %d %d "
#else
			  "0x%08lx 0x%08lx %d %d %p %s%s%s %d %d "
#endif
			  "0x%04x %s %s %s %s\n",
			(u_long)entry->start, (u_long)entry->end,
			resident, privateresident, obj,
			(entry->protection & VM_PROT_READ)?"r":"-",
			(entry->protection & VM_PROT_WRITE)?"w":"-",
			(entry->protection & VM_PROT_EXECUTE)?"x":"-",
			ref_count, shadow_count, flags,
			(entry->eflags & MAP_ENTRY_COW)?"COW":"NCOW",
			(entry->eflags & MAP_ENTRY_NEEDS_COPY)?"NC":"NNC",
			type, fullpath);

		if (obj)
			vm_object_drop(obj);

		if (freepath != NULL) {
			kfree(freepath, M_TEMP);
			freepath = NULL;
		}

		len = strlen(mebuffer);
		if (len > uio->uio_resid) {
			error = EFBIG;
			break;
		}

		/*
		 * We cannot safely hold the map locked while accessing
		 * userspace as a VM fault might recurse the locked map.
		 */
		vm_map_unlock_read(map);
		error = uiomove(mebuffer, len, uio);
		vm_map_lock_read(map);
		if (error)
			break;

		/*
		 * We use map->hint as a poor man's ripout detector.  If
		 * it does not match the entry we set it to prior to
		 * unlocking the map the entry MIGHT now be stale.  In
		 * this case we do an expensive lookup to find our place
		 * in the iteration again.
		 */
		if (map->hint != entry) {
			vm_map_entry_t reentry;

			vm_map_lookup_entry(map, ostart, &reentry);
			entry = reentry;
		}
	}
	vm_map_unlock_read(map);

	return error;
}

struct vobj_scan_data
{
	int *ncolors;
	int n;
};

static int _scan_callback(vm_page_t pg, void *_data)
{
	struct vobj_scan_data *data =
		(struct vobj_scan_data*)_data;
	if (data && data->ncolors)
		if (pg->pc < data->n)
			data->ncolors[pg->pc]++;
	//lwkt_yield();
	return 0;
}

static int _scan_vm_object(struct vm_object *vobj, int *ncolors, int n)
{
	struct vobj_scan_data scan_data;
	if (!vobj || !ncolors || n < 1)
		return 1;
	memset(ncolors, 0, sizeof(*ncolors) * n);
	scan_data.ncolors = ncolors;
	scan_data.n = n;
	vm_object_hold(vobj);
	RB_SCAN(vm_page_rb_tree, &vobj->rb_memq,
			NULL, _scan_callback, &scan_data);
	vm_object_drop(vobj);
	return 0;
}

/*
 * Print the page color counts for resident pages of a process'
 * vm_object set.
 *
 * $ cat /proc/pid/pmap
 * objptr color:pgcount ...
 */
int
procfs_dopmap(struct proc *curp, struct lwp *lp, struct pfsnode *pfs,
	     struct uio *uio)
{
	struct proc *p = lp->lwp_proc;
	int len, c, pos;
	int error = 0, *ncolors = NULL;
	vm_map_t map = &p->p_vmspace->vm_map;
	vm_map_entry_t entry;
	struct vm_object *vobj;
	char mebuffer[MEBUFFERSIZE];

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	if (uio->uio_offset != 0)
		return (0);
	
	ncolors = kmalloc(PQ_L2_SIZE * sizeof(*ncolors),
			M_TEMP, M_WAITOK | M_ZERO);
	if (!ncolors)
		return (ENOMEM);

	vm_map_lock_read(map);
	entry = map->header.next;
	while ((uio->uio_resid > 0) && (entry != &map->header)) {
		KKASSERT(entry);
		pos = len = 0;
		vobj = entry->object.vm_object;
		if (!vobj || vobj->type != OBJT_DEFAULT)
			goto next;
		error = _scan_vm_object(vobj, ncolors, PQ_L2_SIZE);
		if (error)
			break;
		pos += ksnprintf(mebuffer+pos, sizeof(mebuffer)-pos,
				"%p ", vobj);
		for (c = 0; c < PQ_L2_SIZE; c++) {
			if (ncolors[c] <= 0)
				continue;
			pos += ksnprintf(mebuffer+pos,
					sizeof(mebuffer)-pos,
					"%d:%d ", c, ncolors[c]);
			// premature flush
			if ((MEBUFFERSIZE-pos) < 16) {
				vm_map_unlock_read(map);
				mebuffer[pos] = '\0';
				len = strlen(mebuffer);
				error = uiomove(mebuffer, len, uio);
				if (error)
					goto out;
				vm_map_lock_read(map);
				pos = len = 0;
			}
		}
		pos += ksnprintf(mebuffer+pos, sizeof(mebuffer)-pos, "\n");
		vm_map_unlock_read(map);
		mebuffer[pos] = '\0';
		mebuffer[pos-1] = '\n';
		len = strlen(mebuffer);
		error = uiomove(mebuffer, len, uio);
		vm_map_lock_read(map);
		if (error)
			break;
next:;
		entry = entry->next;
	}
	vm_map_unlock_read(map);
out:;
	if (ncolors)
		kfree(ncolors, M_TEMP);
	return error;
}

int
procfs_validmap(struct lwp *lp)
{
	return ((lp->lwp_proc->p_flags & P_SYSTEM) == 0);
}
