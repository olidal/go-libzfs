/* C wrappers around some zfs calls and C in general that should simplify
 * using libzfs from go language, make go code shorter and more readable.
 */

#ifndef SERVERWARE_ZFS_H
#define SERVERWARE_ZFS_H

struct dataset_list {
	zfs_handle_t *zh;
	void *pnext;
};

#ifndef _SYS_ZFS_IOCTL_H
typedef struct zfs_share {
	uint64_t	z_exportdata;
	uint64_t	z_sharedata;
	uint64_t	z_sharetype;	/* 0 = share, 1 = unshare */
	uint64_t	z_sharemax;  /* max length of share string */
} zfs_share_t;

/*
 * A limited number of zpl level stats are retrievable
 * with an ioctl.  zfs diff is the current consumer.
 */
typedef struct zfs_stat {
	uint64_t	zs_gen;
	uint64_t	zs_mode;
	uint64_t	zs_links;
	uint64_t	zs_ctime[2];
} zfs_stat_t;

typedef struct zinject_record {
	uint64_t	zi_objset;
	uint64_t	zi_object;
	uint64_t	zi_start;
	uint64_t	zi_end;
	uint64_t	zi_guid;
	uint32_t	zi_level;
	uint32_t	zi_error;
	uint64_t	zi_type;
	uint32_t	zi_freq;
	uint32_t	zi_failfast;
	char		zi_func[MAXNAMELEN];
	uint32_t	zi_iotype;
	int32_t		zi_duration;
	uint64_t	zi_timer;
	uint64_t	zi_nlanes;
	uint32_t	zi_cmd;
	uint32_t	zi_pad;
} zinject_record_t;

//#endif

//#ifndef _SYS_DMU_H
typedef struct dmu_objset_stats {
	uint64_t dds_num_clones; /* number of clones of this */
	uint64_t dds_creation_txg;
	uint64_t dds_guid;
	dmu_objset_type_t dds_type;
	uint8_t dds_is_snapshot;
	uint8_t dds_inconsistent;
	char dds_origin[ZFS_MAX_DATASET_NAME_LEN];
} dmu_objset_stats_t;

typedef struct zfs_cmd {
	char		zc_name[MAXPATHLEN];	/* name of pool or dataset */
	uint64_t	zc_nvlist_src;		/* really (char *) */
	uint64_t	zc_nvlist_src_size;
	uint64_t	zc_nvlist_dst;		/* really (char *) */
	uint64_t	zc_nvlist_dst_size;
	boolean_t	zc_nvlist_dst_filled;	/* put an nvlist in dst? */
	int		zc_pad2;

	/*
	 * The following members are for legacy ioctls which haven't been
	 * converted to the new method.
	 */
	uint64_t	zc_history;		/* really (char *) */
	char		zc_value[MAXPATHLEN * 2];
	char		zc_string[MAXNAMELEN];
	uint64_t	zc_guid;
	uint64_t	zc_nvlist_conf;		/* really (char *) */
	uint64_t	zc_nvlist_conf_size;
	uint64_t	zc_cookie;
	uint64_t	zc_objset_type;
	uint64_t	zc_perm_action;
	uint64_t	zc_history_len;
	uint64_t	zc_history_offset;
	uint64_t	zc_obj;
	uint64_t	zc_iflags;		/* internal to zfs(7fs) */
	zfs_share_t	zc_share;
	dmu_objset_stats_t zc_objset_stats;
	zinject_record_t zc_inject_record;
	uint32_t	zc_defer_destroy;
	uint32_t	zc_flags;
	uint64_t	zc_action_handle;
	int		zc_cleanup_fd;
	uint8_t		zc_simple;
	uint8_t		zc_pad[3];		/* alignment */
	uint64_t	zc_sendobj;
	uint64_t	zc_fromobj;
	uint64_t	zc_createtxg;
	zfs_stat_t	zc_stat;
} zfs_cmd_t;
#endif

typedef struct dataset_list dataset_list_t;
typedef struct dataset_list* dataset_list_ptr;

sendflags_t *alloc_sendflags();
recvflags_t *alloc_recvflags();



dataset_list_t *create_dataset_list_item();
void dataset_list_close(dataset_list_t *list);
void dataset_list_free(dataset_list_t *list);

dataset_list_t* dataset_list_root();
dataset_list_t* dataset_list_children(dataset_list_t *dataset);
dataset_list_t *dataset_next(dataset_list_t *dataset);

/* Streaming child iterator. Calls zfs_iter_children on `parent` with
 * a fixed C trampoline that hands each child to the Go-side callback
 * goDatasetIterChildrenCallback (declared via //export in zfs.go).
 * The trampoline owns the wrapper's lifecycle: each child's
 * dataset_list_t is freed (closing the underlying zfs_handle_t)
 * after the Go callback returns. Memory peak is O(1) — exactly one
 * child handle alive at any given moment, regardless of total
 * sibling count. Contrast with dataset_list_children, which
 * accumulates the full sibling list before returning.
 *
 * Returns whatever the trampoline returns: 0 on full iteration,
 * non-zero to abort early. The Go side conveys its abort signal
 * (e.g. visit returned an error) by returning non-zero from the
 * callback, which propagates up through here.
 */
int dataset_iter_children_go(dataset_list_ptr parent, uintptr_t go_handle);
int dataset_type(dataset_list_ptr dataset);

dataset_list_ptr dataset_open(const char *path);
int dataset_create(const char *path, zfs_type_t type, nvlist_ptr props);
int dataset_destroy(dataset_list_ptr dataset, boolean_t defer);
zpool_list_ptr dataset_get_pool(dataset_list_ptr dataset);
int dataset_prop_set(dataset_list_ptr dataset, zfs_prop_t prop, const char *value);
int dataset_user_prop_set(dataset_list_ptr dataset, const char *prop, const char *value);
int dataset_clone(dataset_list_ptr dataset, const char *target, nvlist_ptr props);
int dataset_snapshot(const char *path, boolean_t recur, nvlist_ptr props);
int dataset_rollback(dataset_list_ptr dataset, dataset_list_ptr snapshot, boolean_t force);
int dataset_promote(dataset_list_ptr dataset);
int dataset_rename(dataset_list_ptr dataset, const char* new_name, renameflags_t flags);
const char* dataset_is_mounted(dataset_list_ptr dataset);
int dataset_mount(dataset_list_ptr dataset, const char *options, int flags);
int dataset_unmount(dataset_list_ptr dataset, int flags);
int dataset_unmountall(dataset_list_ptr dataset, int flags);
const char *dataset_get_name(dataset_list_ptr ds);

property_list_t *read_dataset_property(dataset_list_t *dataset, int prop);
property_list_t *read_dataset_property_fmt(dataset_list_t *dataset, int prop, int literal);
property_list_t *read_user_property(dataset_list_t *dataset, const char* prop);

const char *go_zfs_prop_column_name(int prop);
int         go_zfs_prop_align_right(int prop);
int         go_zfs_name_to_prop(const char *name);

char** alloc_cstrings(int size);
void strings_setat(char **a, int at, char *v);

struct zfs_cmd *new_zfs_cmd();
int estimate_send_size(struct zfs_cmd *zc);

#endif
/* SERVERWARE_ZFS_H */
