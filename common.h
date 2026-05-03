/* C wrappers around some zfs calls and C in general that should simplify
 * using libzfs from go language, make go code shorter and more readable.
 */

#ifndef loff_t
	#define loff_t off_t
#endif
#define INT_MAX_NAME 256
#define INT_MAX_VALUE 1024
#define	ZAP_OLDMAXVALUELEN 1024
#define	ZFS_MAX_DATASET_NAME_LEN 256

typedef struct property_list {
	char value[INT_MAX_VALUE];
	char source[ZFS_MAX_DATASET_NAME_LEN];
	int property;
	void *pnext;
} property_list_t;

typedef struct libzfs_handle* libzfs_handle_ptr;
typedef struct libpc_handle* libpc_handle_ptr;
typedef struct libpc_handle {
	int lpc_error;
	boolean_t lpc_printerr;
	boolean_t lpc_open_access_error;
	boolean_t lpc_desc_active;
	char lpc_desc[1024];
	pool_config_ops_t *lpc_ops;
	void *lpc_lib_handle;
} libpc_handle_t;
typedef struct nvlist* nvlist_ptr;
typedef struct property_list *property_list_ptr;
typedef struct nvpair* nvpair_ptr;
typedef struct vdev_stat* vdev_stat_ptr;
typedef char* char_ptr;

extern libzfs_handle_ptr libzfsHandle;

int go_libzfs_init();
int go_libpc_init(libpc_handle_t *);

int libzfs_last_error();
const char *libzfs_last_error_str();
const char *libzfs_last_error_action_str();
int libzfs_clear_last_error();

property_list_t *new_property_list();
void free_properties(property_list_t *root);

nvlist_ptr new_property_nvlist();
int property_nvlist_add(nvlist_ptr ptr, const char* prop, const char *value);
int property_nvlist_add_exclude(nvlist_ptr ptr, const char *prop);

/* snap_nvlist_add appends a snapshot name as a boolean nvpair —
 * the input shape zfs_destroy_snaps_nvl wants. Each call adds one
 * snap to the batch; the value is implicit (presence-as-true).
 * The caller frees the nvlist via nvlist_free after the
 * destroy ioctl returns. */
int snap_nvlist_add(nvlist_ptr list, const char *snap);

/* nvlist_free wraps the libnvpair function so Go callers don't
 * need to import its header directly. Pairs with new_property_nvlist
 * for callers that want to free the list explicitly rather than
 * leak it into a defer chain. */
void nvlist_free_go(nvlist_ptr list);

libzfs_handle_ptr new_libzfs_handle();
void free_libzfs_handle(libzfs_handle_ptr h);
const char *libzfs_handle_error_str(libzfs_handle_ptr h);

/* libzfs_handle_set_printerr enables or disables libzfs's auto-print
 * of error messages to stderr on this handle. With print_on_error
 * enabled, libzfs's recv loop prints "cannot receive <prop> property
 * on <ds>: <reason>" lines for each kernel-rejected cmdprop directly
 * to fd 2 (the same path zfs(8) uses by default). Pair with the
 * existing redirect_libzfs_stderr to route those lines to a SCM-
 * passed stderr fd for the bulk v2 recv path. */
void libzfs_handle_set_printerr(libzfs_handle_ptr h, boolean_t enable);

int redirect_libzfs_stdout(int to);
int restore_libzfs_stdout(int saved);
int redirect_libzfs_stderr(int to);
int restore_libzfs_stderr(int saved);

