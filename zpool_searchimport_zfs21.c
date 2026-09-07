//go:build !zfs22

/* zpool_search_import() for the OpenZFS 2.1 line.
 *
 * Only the call itself differs between lines, so only the call lives here --
 * go_zpool_search_import() in zpool.c keeps the importargs and thread-pool
 * setup, in one place, for both.
 *
 * 2.1 signature:
 *   nvlist_t *zpool_search_import(void *, importargs_t *,
 *                                 const pool_config_ops_t *);
 * The config ops are passed alongside an opaque handle.
 */

#include <libzfs.h>
#include <sys/zfs_context.h>
#include <libzutil.h>

#include "common.h"
#include "zpool.h"

nvlist_ptr go_search_import_call(libzfs_handle_ptr zfsh, importargs_t *idata) {
	return zpool_search_import(zfsh, idata, &libzfs_config_ops);
}
