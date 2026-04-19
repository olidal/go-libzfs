#ifndef ZFS_STREAM_H
#define ZFS_STREAM_H

#include <libzfs.h>


#define DRR_STRUCT_SIZE sizeof(dmu_replay_record_t)
#define DRR_BEGIN_STRUCT_SIZE sizeof(struct dmu_begin)
/* 
 * struct ddr_checksum is the largest of the drr union
 * therefore its size is dimensioning the whole union 
 */
#define DRR_CKSUM_STRUCT_SIZE sizeof(struct drr_checksum)
#define DRR_UNION_SIZE (DRR_CKSUM_STRUCT_SIZE)
#endif

typedef struct zfs_stream_config {
    boolean_t   do_cksum;
    boolean_t   do_byteswap;
    boolean_t   discover_byteorder;
    char *      stream_error;
    zio_cksum_t cksum;
    uint64_t    total_stream_len;
    dmu_replay_record_t thedrr;
} zfs_stream_config_t;
typedef zfs_stream_config_t *zfs_stream_config_ptr;

typedef struct drr_begin drr_begin_t;
 
extern zfs_stream_config_ptr *init_stream(
    boolean_t do_cksum, 
    boolean_t do_byteswap, 
    boolean_t discover_byteswap);

extern void *read_dmu_replay_record(int fd);
extern size_t
read_hdr(int fd, zfs_stream_config_ptr *zsc);

inline drr_begin_t *drr_get_begin(dmu_replay_record_t *drr_ptr) {
    return &(drr_ptr->drr_u.drr_begin);
}