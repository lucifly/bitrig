/*	$OpenBSD: vos_local.h,v 1.2 1999/04/30 01:59:06 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      H�gskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * RCSID("$KTH: vos_local.h,v 1.7 1999/03/14 19:05:57 assar Exp $");
 */

/* Iflags returned by AFSVolMonitor */

#define ITOffline     0x1
#define ITBusy        0x2
#define ITReadOnly    0x8
#define ITCreate      0x10
#define ITCreateVolID 0x1000

#define LISTVOL_PART      0x1
#define LISTVOL_NOAUTH    0x2
#define LISTVOL_LOCALAUTH 0x4
#define LISTVOL_FAST      0x8

/* this program needs __progname defined as a macro */
#define __progname "vos"
#define PROGNAME (vos_interactive ? "" : __progname" ")

/* if this is set the program runs in interactive mode */
extern int vos_interactive;

int vos_examine (int, char **);
int vos_listpart (int, char **);
int vos_listvol (int, char **);
int vos_listvldb(int argc, char **argv);
int vos_partinfo (int, char **);
int vos_status (int, char **);
int vos_createentry (int, char **);
int vos_syncsite (int, char **);
int vos_dump (int, char **);
int vos_create(int argc, char **argv);
int vos_endtrans(int argc, char **argv);

int vos_listvldb_iter (const char *host, const char *cell,
		       arlalib_authflags_t auth, 
		       int (*proc)(void *data, struct vldbentry *),
		       void *data);

int partition_num2name(int id, char *str, size_t sz);
int partition_name2num(const char *name);
const char *getvolumetype(int32_t flag);
const char *getvolumetype2(int32_t type);
int getlistparts(const char *cell, const char *host,
		 part_entries *parts, arlalib_authflags_t auth);
int printlistvol(const char *cell, const char *host, int part, int flags,
		 arlalib_authflags_t auth);
int get_vlentry (const char *cell, const char *host, const char *volname,
		 arlalib_authflags_t auth, nvldbentry *nvlentry);
int new_vlentry (struct rx_connection *conn, const char *cell, const char *host,
		 nvldbentry *nvldbentry, arlalib_authflags_t auth);



int vos_createvolume (char *host, int32_t part, char *cell, 
		      arlalib_authflags_t auth,
		      char *name, int verbose);
int vos_endtransaction (const char *cell, const char *host,
			int32_t trans, arlalib_authflags_t auth, int verbose);

void find_db_cell_and_host (const char **cell, const char **host);
