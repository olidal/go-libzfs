//go:build zfs22

/* zpool_search_import() for the OpenZFS 2.2 line.
 *
 * 2.2 folded the config ops into a handle it now publishes, so the signature
 * lost an argument:
 *   nvlist_t *zpool_search_import(libpc_handle_t *, importargs_t *);
 *
 * struct libpc_handle is public in 2.2's libzutil.h -- in 2.1 it was internal,
 * which is why this file cannot be built there and why the fork used to carry
 * a private copy of the struct (removed 2026-09-07: it fed a helper whose
 * result nothing read).
 *
 * lpc_printerr matches 2.1's behaviour, where zpool_search_import printed its
 * own errors: the argument moved, the behaviour should not.
 */

#include <libzfs.h>
#include <sys/zfs_context.h>
#include <libzutil.h>
#include <string.h>

#include "common.h"
#include "zpool.h"

nvlist_ptr go_search_import_call(libzfs_handle_ptr zfsh, importargs_t *idata) {
	libpc_handle_t lpch;
	memset(&lpch, 0, sizeof(lpch));
	lpch.lpc_lib_handle = zfsh;
	lpch.lpc_ops = &libzfs_config_ops;
	lpch.lpc_printerr = B_TRUE;
	return zpool_search_import(&lpch, idata);
}
