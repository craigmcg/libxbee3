/*
	libxbee - a C library to aid the use of Digi's XBee wireless modules
	          running in API mode (AP=2).

	Copyright (C) 2009	Attie Grande (attie@attie.co.uk)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.	If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "tx.h"
#include "xbee_int.h"
#include "log.h"
#include "conn.h"
#include "mode.h"
#include "ll.h"

xbee_err xbee_txAlloc(struct xbee_txInfo **nInfo) {
	size_t memSize;
	struct xbee_txInfo *info;
	
	if (!nInfo) return XBEE_EMISSINGPARAM;
	
	memSize = sizeof(*info);
	
	if (!(info = malloc(memSize))) return XBEE_ENOMEM;
	
	memset(info, 0, memSize);
	info->bufList = ll_alloc();
	xsys_sem_init(&info->sem);
	
	*nInfo = info;
	
	return XBEE_ENONE;
}

xbee_err xbee_txFree(struct xbee_txInfo *info) {
	if (!info) return XBEE_EMISSINGPARAM;
	
	ll_free(info->bufList, (void(*)(void*))xbee_pktFree);
	xsys_sem_destroy(&info->sem);
	free(info);
	
	return XBEE_ENONE;
}

/* ######################################################################### */

xbee_err xbee_tx(struct xbee *xbee, int *restart, void *arg) {
	xbee_err ret;
	struct xbee_buf *buf;
	xbee_err (*tx)(struct xbee *xbee, struct xbee_buf *buf);
	
	tx = arg;
	
	while (!xbee->die) {
		if (xsys_sem_wait(&xbee->tx->sem) != 0) return XBEE_ESEMAPHORE;
		if (ll_ext_head(xbee->tx->bufList, (void**)&buf) != XBEE_ENONE) return XBEE_ELINKEDLIST;
		if (!buf) continue;
		
		if ((ret = tx(xbee, buf)) != XBEE_ENONE) {
			xbee_log(1, "tx() returned %d... buffer was lost", ret);
			continue;
		}
		
		free(buf);
	}
	
	return XBEE_ENONE;
}

/* ######################################################################### */

xbee_err xbee_txHandler(struct xbee *xbee, struct xbee_con *con, struct xbee_buf *iBuf) {
	xbee_err ret;
	struct xbee_buf *oBuf;
	
	if ((ret = con->conType->txHandler->func(xbee, &con->address, iBuf, &oBuf)) != XBEE_ENONE) return ret;
	
	if (ll_add_tail(xbee->tx->bufList, oBuf) != XBEE_ENONE) return XBEE_ELINKEDLIST;
	if (xsys_sem_post(&xbee->tx->sem) != 0) return XBEE_ESEMAPHORE;
	
	return XBEE_ENONE;
}