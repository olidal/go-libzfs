/* C helpers for zfs permission delegation (zfs allow / unallow).
 * Wraps zfs_set_fsacl / zfs_get_fsacl with a simplified API:
 *   - user who_type only (no groups, everyone, permission-sets, or create-time)
 *   - perms given as comma-separated string
 *   - locality: 1=local, 2=descendent, 3=both
 */

#ifndef GO_LIBZFS_ZFS_PERM_H
#define GO_LIBZFS_ZFS_PERM_H

/* Include order: library headers must come before these project headers. */
#include "zfs.h"

/* Locality bitmask values (match Go DelegPermLocality). */
#define GO_PERM_LOCAL      1
#define GO_PERM_DESCENDENT 2

/* Linked-list of delegation entries returned by go_zfs_get_allow. */
typedef struct go_deleg_entry {
	char  user[256];     /* username (resolved from uid) or raw uid string */
	char  perms[1024];   /* comma-separated permissions */
	int   locality;      /* GO_PERM_LOCAL | GO_PERM_DESCENDENT */
	struct go_deleg_entry *next;
} go_deleg_entry_t;

/* Apply delegation change on a dataset handle.
 * who:    username or numeric uid string
 * perms:  comma-separated permission names (e.g. "send,snapshot,hold")
 * locality: GO_PERM_LOCAL | GO_PERM_DESCENDENT (bitmask)
 * unset:  0 = grant (allow), 1 = revoke (unallow)
 * Returns 0 on success, non-zero on error (errno or libzfs error).
 */
int go_zfs_user_allow(dataset_list_ptr ds, const char *who, const char *perms,
    int locality, int unset);

/* Read current delegations. Returns a linked list the caller must free with
 * go_zfs_free_deleg_entries. *out is set to NULL if there are no entries.
 * Returns 0 on success, non-zero on error.
 */
int go_zfs_get_allow(dataset_list_ptr ds, go_deleg_entry_t **out);

void go_zfs_free_deleg_entries(go_deleg_entry_t *head);

#endif
