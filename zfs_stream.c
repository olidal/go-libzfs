//go:build libzfs_source

#include <libzfs.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libzutil.h>
#include <sys/zio_compress.h>
#include <sys/zfs_ioctl.h>
#include <zfs_fletcher.h>

#include "common.h"
#include "zpool.h"
#include "zfs.h"
#include "zfs_stream.h"

zfs_stream_config_ptr *init_stream(boolean_t do_cksum, boolean_t do_byteswap, boolean_t discover_byteswap) {
    zfs_stream_config_ptr zsc_ptr = calloc(1,sizeof(zfs_stream_config_t));
    if (! zsc_ptr) return NULL;
    zsc_ptr -> do_cksum = do_cksum;
    zsc_ptr -> do_byteswap = do_byteswap;
    zsc_ptr -> discover_byteorder = B_TRUE;
    zsc_ptr -> stream_error = NULL;
    zsc_ptr -> total_stream_len = 0;
    //ZIO_SET_CHECKSUM(&(zsc_ptr.cksum), 0, 0, 0, 0);
    return zsc_ptr;
}

//struct drr_begin_t *drr_get_begin(dmu_replay_record_t *drr_ptr)
//{
//    return &(drr_ptr->drr_u.drr_begin);
//}

/*
 * ssread - send stream read.
 *
 * Read while computing incremental checksum
 */
static size_t
ssread(int fd, void *buf, size_t len, zfs_stream_config_ptr *zsc)
{
    size_t outlen;  
    FILE *f = fdopen(fd,"r");

    if ((outlen = fread(buf, len, 1, f)) == 0)
        return (0);

    if ((*zsc)->discover_byteorder) {
        // This only happens when reading the first DRR_BEGIN record

        (*zsc)->stream_error = NULL; // No error yet
        dmu_replay_record_t *drr_buf = (dmu_replay_record_t *)buf;
        struct drr_begin *drrb = &(drr_buf->drr_u.drr_begin);
        if (drrb->drr_magic == BSWAP_64(DMU_BACKUP_MAGIC)) {
            (*zsc)->do_byteswap = B_TRUE;
        }
        else if (drrb->drr_magic != DMU_BACKUP_MAGIC) {
            (*zsc)->stream_error = "Not a ZFS stream. Aborting. ";
            return 0;
        } else {
            (*zsc)->do_byteswap = B_FALSE;
        }
        (*zsc)->discover_byteorder = B_FALSE;
    }

    if ((*zsc)->do_cksum)
    {
        if ((*zsc)->do_byteswap)
            fletcher_4_incremental_byteswap(buf, len, &((*zsc)->cksum));
        else
            fletcher_4_incremental_native(buf, len, &((*zsc)->cksum));
    }
    (*zsc) -> total_stream_len += len;
    return (outlen);
}

size_t
read_hdr(int fd,  zfs_stream_config_ptr *zsc)
{
    fprintf(stderr, "Before asserts...");
    fflush(stderr);
    ASSERT3U(offsetof(dmu_replay_record_t, drr_u.drr_checksum.drr_checksum),
             ==, sizeof(dmu_replay_record_t) - sizeof(zio_cksum_t));
    assert(zsc != NULL);
    assert(*zsc != NULL);
    dmu_replay_record_t *drr = &((*zsc)->thedrr);

    fprintf(stderr, "Calling ssread...");
    fflush(stderr);
    size_t r = ssread(fd, drr, sizeof(*drr) - sizeof(zio_cksum_t), zsc);
    if (r == 0)
        return (0);
    fprintf(stderr, "Read up to checksum...");
    fflush(stderr);
    zio_cksum_t saved_cksum = (*zsc)->cksum;
    r = ssread(fd, &drr->drr_u.drr_checksum.drr_checksum,
               sizeof(zio_cksum_t), zsc);
    if (r == 0)
        return (0);
    fprintf(stderr, "Read checksum...");
    if ((*zsc)->do_cksum &&
        !ZIO_CHECKSUM_IS_ZERO(&drr->drr_u.drr_checksum.drr_checksum) &&
        !ZIO_CHECKSUM_EQUAL(saved_cksum,
                            drr->drr_u.drr_checksum.drr_checksum))
    {
        (*zsc) -> stream_error = "invalid checksum";
        (void)fprintf(stderr,"Incorrect checksum in record header.\n");
        (void)fprintf(stderr,"Expected checksum = %llx/%llx/%llx/%llx\n",
                     (longlong_t)saved_cksum.zc_word[0],
                     (longlong_t)saved_cksum.zc_word[1],
                     (longlong_t)saved_cksum.zc_word[2],
                     (longlong_t)saved_cksum.zc_word[3]);
        return (0);
    }
    return (sizeof(*drr));
}

