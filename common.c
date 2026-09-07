#include <libzfs.h>
#include <libzutil.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>

#include "common.h"

libzfs_handle_ptr libzfsHandle;

int go_libzfs_init() {
	libzfsHandle = libzfs_init();
	return 0;
}

int libzfs_last_error() {
	return libzfs_errno(libzfsHandle);
}

const char *libzfs_last_error_str() {
	return libzfs_error_description(libzfsHandle);
}

/* Returns libzfs's action string for the last error — e.g. "cannot destroy
 * 'pool/foo'". Combined with libzfs_last_error_str() it produces the exact
 * line that the zfs(8) CLI prints to stderr. Empty string if none is set. */
const char *libzfs_last_error_action_str() {
	const char *a = libzfs_error_action(libzfsHandle);
	if (a == NULL) return "";
	return a;
}

int libzfs_clear_last_error() {
	zfs_standard_error(libzfsHandle, EZFS_SUCCESS, "success");
	return 0;
}

property_list_t *new_property_list() {
	property_list_t *r = malloc(sizeof(property_list_t));
	memset(r, 0, sizeof(property_list_t));
	return r;
}

void free_properties(property_list_t *root) {
	property_list_t *tmp = 0;
	while(root) {
		tmp = root->pnext;
		free(root);
		root = tmp;
	}
}

nvlist_ptr new_property_nvlist() {
	nvlist_ptr props = NULL;
	int r = nvlist_alloc(&props, NV_UNIQUE_NAME, 0);
	if ( r != 0 ) {
		return NULL;
	}
	return props;
}

int property_nvlist_add(nvlist_ptr list, const char *prop, const char *value) {
	return nvlist_add_string(list, prop, value);
}

/* property_nvlist_add_exclude encodes a `zfs recv -x <prop>` request.
 * libzfs_sendrecv.c treats a DATA_TYPE_BOOLEAN nvpair in the recvprops
 * nvlist as an instruction to strip that property from the incoming
 * stream, so the value never lands on the destination dataset. */
int property_nvlist_add_exclude(nvlist_ptr list, const char *prop) {
	return nvlist_add_boolean(list, prop);
}

/* snap_nvlist_add appends a snapshot's full name as a boolean nvpair
 * to the destroy-snaps batch nvlist. zfs_destroy_snaps_nvl ignores
 * the value side and treats every boolean entry as "destroy this
 * snapshot in the same kernel transaction", matching zfs(8)'s
 * recursive-destroy implementation in cmd/zfs/zfs_iter.c +
 * lib/libzfs/libzfs_dataset.c. */
int snap_nvlist_add(nvlist_ptr list, const char *snap) {
	return nvlist_add_boolean(list, snap);
}

/* nvlist_free_go is a thin wrapper so Go callers can release the
 * nvlist they built via new_property_nvlist without importing
 * sys/nvpair.h. Identical semantics to libnvpair's nvlist_free. */
void nvlist_free_go(nvlist_ptr list) {
	if (list != NULL) {
		nvlist_free(list);
	}
}

/* new_libzfs_handle allocates a fresh libzfs handle, distinct from
 * the package-global libzfsHandle. Daemons that serve concurrent
 * operations should pair each long-running call (notably zfs_receive)
 * with its own handle so userspace state — property tables, scratch
 * nvlist buffers, the zfs_handle_t cache — is per-call rather than
 * shared across concurrent invocations. Mirrors the per-process
 * isolation that zfs(8) gets implicitly. */
libzfs_handle_ptr new_libzfs_handle() {
	return libzfs_init();
}

/* free_libzfs_handle releases a handle obtained via new_libzfs_handle.
 * Safe on NULL. */
void free_libzfs_handle(libzfs_handle_ptr h) {
	if (h != NULL) {
		libzfs_fini(h);
	}
}

/* libzfs_handle_error_str returns the error description recorded on
 * the supplied handle (rather than the global). Required when a
 * caller-provided handle's last operation failed and the process-wide
 * libzfsHandle's state isn't relevant. */
const char *libzfs_handle_error_str(libzfs_handle_ptr h) {
	return libzfs_error_description(h);
}

/* libzfs_handle_set_printerr toggles per-handle "print errors to
 * stderr" mode. zfs(8) enables this on its g_zfs handle so libzfs
 * functions like zfs_setprop_error fprintf their formatted error
 * directly. We use it on the bulk v2 recv path to surface per-
 * property kernel-rejection messages — same shape zfs(8) prints. */
void libzfs_handle_set_printerr(libzfs_handle_ptr h, boolean_t enable) {
	libzfs_print_on_error(h, enable);
}

int redirect_libzfs_stdout(int to) {
	int save, res;
	save = dup(STDOUT_FILENO);
	if (save < 0) {
		return save;
	}
	res = dup2(to, STDOUT_FILENO);
	if (res < 0) {
		return res;
	}
	return save;
}

int restore_libzfs_stdout(int saved) {
	int res;
	fflush(stdout);
	res = dup2(saved, STDOUT_FILENO);
	if (res < 0) {
		return res;
	}
	close(saved);
}

/* Same dance for stderr (fd 2). libzfs writes some verbose output to
 * stderr (e.g. "skipping snapshot ..." during -nv send walks) which
 * stdout-only redirects miss. Callers redirect both fds to the same
 * pipe to capture the full narration in emission order. */
int redirect_libzfs_stderr(int to) {
	int save, res;
	save = dup(STDERR_FILENO);
	if (save < 0) {
		return save;
	}
	res = dup2(to, STDERR_FILENO);
	if (res < 0) {
		return res;
	}
	return save;
}

int restore_libzfs_stderr(int saved) {
	int res;
	fflush(stderr);
	res = dup2(saved, STDERR_FILENO);
	if (res < 0) {
		return res;
	}
	close(saved);
	return 0;
}
