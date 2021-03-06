/*	$OpenBSD: vmctl.c,v 1.12 2016/01/26 07:58:35 reyk Exp $	*/

/*
 * Copyright (c) 2014 Mike Larkin <mlarkin@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <machine/vmmvar.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <imsg.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vmd.h"
#include "vmctl.h"

extern char *__progname;
uint32_t info_id;
char info_name[VMM_MAX_NAME_LEN];
int info_console;

/*
 * start_vm
 *
 * Request vmd to start the VM defined by the supplied parameters
 *
 * Parameters:
 *  name: optional name of the VM
 *  memsize: memory size (MB) of the VM to create
 *  nnics: number of vionet network interfaces to create
 *  ndisks: number of disk images
 *  disks: disk image file names
 *  kernel: kernel image to load
 *
 * Return:
 *  0 if the request to start the VM was sent successfully.
 *  ENOMEM if a memory allocation failure occurred.
 */
int
start_vm(const char *name, int memsize, int nnics, int ndisks, char **disks,
    char *kernel)
{
	struct vm_create_params *vcp;
	int i;

	vcp = malloc(sizeof(struct vm_create_params));
	if (vcp == NULL)
		return (ENOMEM);

	bzero(vcp, sizeof(struct vm_create_params));

	vcp->vcp_memory_size = memsize;
	vcp->vcp_ncpus = 1;
	vcp->vcp_ndisks = ndisks;

	for (i = 0 ; i < ndisks; i++)
		strlcpy(vcp->vcp_disks[i], disks[i], VMM_MAX_PATH_DISK);

	if (name != NULL)
		strlcpy(vcp->vcp_name, name, VMM_MAX_NAME_LEN);
	strlcpy(vcp->vcp_kernel, kernel, VMM_MAX_KERNEL_PATH);
	vcp->vcp_nnics = nnics;

	imsg_compose(ibuf, IMSG_VMDOP_START_VM_REQUEST, 0, 0, -1,
	    vcp, sizeof(struct vm_create_params));

	free(vcp);
	return (0);
}

/*
 * start_vm_complete
 *
 * Callback function invoked when we are expecting an
 * IMSG_VMDOP_START_VMM_RESPONSE message indicating the completion of
 * a start vm operation.
 *
 * Parameters:
 *  imsg : response imsg received from vmd
 *  ret  : return value
 *  autoconnect : open the console after startup
 *
 * Return:
 *  Always 1 to indicate we have processed the return message (even if it
 *  was an incorrect/failure message) 
 *
 *  The function also sets 'ret' to the error code as follows:
 *   0     : Message successfully processed
 *   EINVAL: Invalid or unexpected response from vmd
 *   EIO   : start_vm command failed
 */
int
start_vm_complete(struct imsg *imsg, int *ret, int autoconnect)
{
	struct vmop_result *vmr;
	int res;

	if (imsg->hdr.type == IMSG_VMDOP_START_VM_RESPONSE) {
		vmr = (struct vmop_result *)imsg->data;
		res = vmr->vmr_result;
		if (res) {
			errno = res;
			warn("start vm command failed");
			*ret = EIO;
		} else if (autoconnect) {
			/* does not return */
			ctl_openconsole(vmr->vmr_ttyname);
		} else {
			warnx("started vm %d successfully, tty %s",
			    vmr->vmr_id, vmr->vmr_ttyname);
			*ret = 0;
		}
	} else {
		warnx("unexpected response received from vmd");
		*ret = EINVAL;
	}

	return (1);
}

/*
 * terminate_vm
 *
 * Request vmd to stop the VM indicated
 *
 * Parameters:
 *  terminate_id: ID of the vm to be terminated
 *  name: optional name of the VM to be terminated
 */
void
terminate_vm(uint32_t terminate_id, const char *name)
{
	struct vmop_id vid;

	memset(&vid, 0, sizeof(vid));
	vid.vid_id = terminate_id;
	if (name != NULL)
		(void)strlcpy(vid.vid_name, name, sizeof(vid.vid_name));

	imsg_compose(ibuf, IMSG_VMDOP_TERMINATE_VM_REQUEST, 0, 0, -1,
	    &vid, sizeof(vid));
}

/*
 * terminate_vm_complete
 *
 * Callback function invoked when we are expecting an
 * IMSG_VMDOP_TERMINATE_VMM_RESPONSE message indicating the completion of
 * a terminate vm operation.
 *
 * Parameters:
 *  imsg : response imsg received from vmd
 *  ret  : return value
 *
 * Return:
 *  Always 1 to indicate we have processed the return message (even if it
 *  was an incorrect/failure message) 
 *
 *  The function also sets 'ret' to the error code as follows:
 *   0     : Message successfully processed
 *   EINVAL: Invalid or unexpected response from vmd
 *   EIO   : terminate_vm command failed
 */
int
terminate_vm_complete(struct imsg *imsg, int *ret)
{
	struct vmop_result *vmr;
	int res;

	if (imsg->hdr.type == IMSG_VMDOP_TERMINATE_VM_RESPONSE) {
		vmr = (struct vmop_result *)imsg->data;
		res = vmr->vmr_result;
		if (res) {
			errno = res;
			warn("terminate vm command failed");
			*ret = EIO;
		} else {
			warnx("terminated vm %d successfully", vmr->vmr_id);
			*ret = 0;
		}
	} else {
		warnx("unexpected response received from vmd");
		*ret = EINVAL;
	}

	return (1);
}

/*
 * get_info_vm
 *
 * Return the list of all running VMs or find a specific VM by ID or name.
 *
 * Parameters:
 *  id: optional ID of a VM to list
 *  name: optional name of a VM to list 
 *  console: if true, open the console of the selected VM (by name or ID)
 *
 * Request a list of running VMs from vmd
 */
void
get_info_vm(uint32_t id, const char *name, int console)
{
	info_id = id;
	if (name != NULL)
		(void)strlcpy(info_name, name, sizeof(info_name));
	info_console = console;
	imsg_compose(ibuf, IMSG_VMDOP_GET_INFO_VM_REQUEST, 0, 0, -1, NULL, 0);
}

/*
 * chec_info_id
 *
 * Check if requested name or ID of a VM matches specified arguments
 *
 * Parameters:
 *  name: name of the VM
 *  id: ID of the VM
 */
int
check_info_id(const char *name, uint32_t id)
{
	if (info_id == 0 && *info_name == '\0')
		return (-1);
	if (info_id != 0 && info_id == id)
		return (1);
	if (*info_name != '\0' && name && strcmp(info_name, name) == 0)
		return (1);
	return (0);
}

/*
 * add_info
 *
 * Callback function invoked when we are expecting an
 * IMSG_VMDOP_GET_INFO_VM_DATA message indicating the receipt of additional
 * "list vm" data, or an IMSG_VMDOP_GET_INFO_VM_END_DATA message indicating
 * that no additional "list vm" data will be forthcoming.
 *
 * Parameters:
 *  imsg : response imsg received from vmd
 *  ret  : return value
 *
 * Return:
 *  0     : the returned data was successfully added to the "list vm" data.
 *          The caller can expect more data.
 *  1     : IMSG_VMDOP_GET_INFO_VM_END_DATA received (caller should not call
 *          add_info again), or an error occurred adding the returned data
 *          to the "list vm" data. The caller should check the value of
 *          'ret' to determine which case occurred.
 *
 * This function does not return if a VM is found and info_console is set.
 *
 *  The function also sets 'ret' to the error code as follows:
 *   0     : Message successfully processed
 *   EINVAL: Invalid or unexpected response from vmd
 *   ENOMEM: memory allocation failure
 */
int
add_info(struct imsg *imsg, int *ret)
{
	static size_t ct = 0;
	static struct vmop_info_result *vir = NULL;

	if (imsg->hdr.type == IMSG_VMDOP_GET_INFO_VM_DATA) {
		vir = reallocarray(vir, ct + 1,
		    sizeof(struct vmop_info_result));
		if (vir == NULL) {
			*ret = ENOMEM;
			return (1);
		}
		memcpy(&vir[ct], imsg->data, sizeof(struct vmop_info_result));
		ct++;
		*ret = 0;
		return (0);
	} else if (imsg->hdr.type == IMSG_VMDOP_GET_INFO_VM_END_DATA) {
		if (info_console)
			vm_console(vir, ct);
		else
			print_vm_info(vir, ct);
		free(vir);
		*ret = 0;
		return (1);
	} else {
		*ret = EINVAL;
		return (1);
	}
}

/*
 * print_vm_info
 *
 * Prints the vm information returned from vmd in 'list' to stdout.
 *
 * Parameters
 *  list: the vm information (consolidated) returned from vmd via imsg
 *  ct  : the size (number of elements in 'list') of the result
 */
void
print_vm_info(struct vmop_info_result *list, size_t ct)
{
	struct vm_info_result *vir;
	size_t i, j;
	char *vcpu_state;

	printf("%5s %5s %5s %9s %9s %*s %s\n", "ID", "PID", "VCPUS", "MAXMEM",
	    "CURMEM", VM_TTYNAME_MAX, "TTY", "NAME");
	for (i = 0; i < ct; i++) {
		vir = &list[i].vir_info;
		if (check_info_id(vir->vir_name, vir->vir_id)) {
			printf("%5u %5u %5zd %7zdMB %7zdMB %*s %s\n",
			    vir->vir_id, vir->vir_creator_pid,
			    vir->vir_ncpus, vir->vir_memory_size,
			    vir->vir_used_size / 1024 / 1024 , VM_TTYNAME_MAX,
			    list[i].vir_ttyname, vir->vir_name);
		}
		if (check_info_id(vir->vir_name, vir->vir_id) > 0) {
			for (j = 0; j < vir->vir_ncpus; j++) {
				if (vir->vir_vcpu_state[j] ==
				    VCPU_STATE_STOPPED)
					vcpu_state = "STOPPED";
				else if (vir->vir_vcpu_state[j] ==
				    VCPU_STATE_RUNNING)
					vcpu_state = "RUNNING";
				else
					vcpu_state = "UNKNOWN";

				printf(" VCPU: %2zd STATE: %s\n",
				    j, vcpu_state);
			}
		}
	}
}

/*
 * vm_console
 *
 * Connects to the vm console returned from vmd in 'list'.
 *
 * Parameters
 *  list: the vm information (consolidated) returned from vmd via imsg
 *  ct  : the size (number of elements in 'list') of the result
 */
__dead void
vm_console(struct vmop_info_result *list, size_t ct)
{
	struct vmop_info_result *vir;
	size_t i;

	for (i = 0; i < ct; i++) {
		vir = &list[i];
		if (check_info_id(vir->vir_info.vir_name,
		    vir->vir_info.vir_id) > 0) {
			/* does not return */
			ctl_openconsole(vir->vir_ttyname);
		}
	}

	errx(1, "console %d not found", info_id);
}

/*
 * create_imagefile
 *
 * Create an empty imagefile with the specified path and size.
 *
 * Parameters:
 *  imgfile_path: path to the image file to create
 *  imgsize     : size of the image file to create (in MB)
 *
 * Return:
 *  EEXIST: The requested image file already exists
 *  0     : Image file successfully created
 *  Exxxx : Various other Exxxx errno codes due to other I/O errors
 */
int
create_imagefile(const char *imgfile_path, long imgsize)
{
	int fd, ret;

	/* Refuse to overwrite an existing image */
	fd = open(imgfile_path, O_RDWR | O_CREAT | O_TRUNC | O_EXCL,
	    S_IRUSR | S_IWUSR);
	if (fd == -1)
		return (errno);

	/* Extend to desired size */
	if (ftruncate(fd, (off_t)imgsize * 1024 * 1024) == -1) {
		ret = errno;
		close(fd);
		unlink(imgfile_path);
		return (ret);
	}

	ret = close(fd);
	return (ret);
}
