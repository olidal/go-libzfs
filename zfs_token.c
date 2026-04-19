//go:build libzfs_source

#include <libzfs.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <libzutil.h>
#include <sys/zio_compress.h>
#include <zfs_fletcher.h>


#include "common.h"
#include "zpool.h"
#include "zfs.h"

const int ZFS_SEND_RESUME_TOKEN_VERSION = 1;

// Copied from zfs/module/zfs/dsl_dataset.c
char *
forge_receive_resume_token(
        uint64_t fromguid,
        uint64_t object,
        uint64_t offset,
        uint64_t bytes,
        uint64_t toguid,
        char *toname,
        boolean_t largeblockok,
        boolean_t embedok,
        boolean_t compressok,
        boolean_t rawok)
{
    char *str;
    void *packed;
    uint8_t *compressed;
    uint64_t val;
    nvlist_t *token_nv = fnvlist_alloc();
    size_t packed_size, compressed_size;
    fnvlist_add_uint64(token_nv, "fromguid", fromguid);
    fnvlist_add_uint64(token_nv, "object", object);
    fnvlist_add_uint64(token_nv, "offset", offset);
    fnvlist_add_uint64(token_nv, "bytes", bytes);
    fnvlist_add_uint64(token_nv, "toguid", toguid);
    fnvlist_add_string(token_nv, "toname", toname);
    if (largeblockok)
        fnvlist_add_boolean(token_nv, "largeblockok");
    if (embedok)
        fnvlist_add_boolean(token_nv, "embedok");
    if (compressok)
        fnvlist_add_boolean(token_nv, "compressok");
    if (rawok)
        fnvlist_add_boolean(token_nv, "rawok");
    packed = fnvlist_pack(token_nv, &packed_size);
    fnvlist_free(token_nv);
    compressed = calloc(1, packed_size);
    compressed_size = gzip_compress(packed, compressed,
                                    packed_size, packed_size, 6);
    zio_cksum_t cksum;
    fletcher_4_native_varsize(compressed, compressed_size, &cksum);

    size_t alloc_size = compressed_size * 2 + 1;
    str = calloc(1, alloc_size);
    for (int i = 0; i < compressed_size; i++)
    {
        size_t offset = i * 2;
        (void)snprintf(str + offset, alloc_size - offset,
                       "%02x", compressed[i]);
    }
    str[compressed_size * 2] = '\0';
    size_t propval_len = strlen(str) + 2 * 16 + 1 + 3 + 1;
    char *propval = malloc(propval_len);
    snprintf(propval, propval_len, "%u-%llx-%llx-%s",
            ZFS_SEND_RESUME_TOKEN_VERSION,
            (longlong_t)cksum.zc_word[0],
            (longlong_t)packed_size, str);
    free(packed);
    free(str);
    free(compressed);
    return (propval);
}
/*
 * Returns a string that represents the receive resume stats token. It should
 * be freed with strfree().
 */
/*
char *
get_receive_resume_stats_impl(dsl_dataset_t *ds)
{
    dsl_pool_t *dp = ds->ds_dir->dd_pool;

    if (dsl_dataset_has_resume_receive_state(ds))
    {
        char *str;
        void *packed;
        uint8_t *compressed;
        uint64_t val;
        nvlist_t *token_nv = fnvlist_alloc();
        size_t packed_size, compressed_size;

        if (zap_lookup(dp->dp_meta_objset, ds->ds_object,
                       DS_FIELD_RESUME_FROMGUID, sizeof(val), 1, &val) == 0)
        {
            fnvlist_add_uint64(token_nv, "fromguid", val);
        }
        if (zap_lookup(dp->dp_meta_objset, ds->ds_object,
                       DS_FIELD_RESUME_OBJECT, sizeof(val), 1, &val) == 0)
        {
            fnvlist_add_uint64(token_nv, "object", val);
        }
        if (zap_lookup(dp->dp_meta_objset, ds->ds_object,
                       DS_FIELD_RESUME_OFFSET, sizeof(val), 1, &val) == 0)
        {
            fnvlist_add_uint64(token_nv, "offset", val);
        }
        if (zap_lookup(dp->dp_meta_objset, ds->ds_object,
                       DS_FIELD_RESUME_BYTES, sizeof(val), 1, &val) == 0)
        {
            fnvlist_add_uint64(token_nv, "bytes", val);
        }
        if (zap_lookup(dp->dp_meta_objset, ds->ds_object,
                       DS_FIELD_RESUME_TOGUID, sizeof(val), 1, &val) == 0)
        {
            fnvlist_add_uint64(token_nv, "toguid", val);
        }
        char buf[MAXNAMELEN];
        if (zap_lookup(dp->dp_meta_objset, ds->ds_object,
                       DS_FIELD_RESUME_TONAME, 1, sizeof(buf), buf) == 0)
        {
            fnvlist_add_string(token_nv, "toname", buf);
        }
        if (zap_contains(dp->dp_meta_objset, ds->ds_object,
                         DS_FIELD_RESUME_LARGEBLOCK) == 0)
        {
            fnvlist_add_boolean(token_nv, "largeblockok");
        }
        if (zap_contains(dp->dp_meta_objset, ds->ds_object,
                         DS_FIELD_RESUME_EMBEDOK) == 0)
        {
            fnvlist_add_boolean(token_nv, "embedok");
        }
        if (zap_contains(dp->dp_meta_objset, ds->ds_object,
                         DS_FIELD_RESUME_COMPRESSOK) == 0)
        {
            fnvlist_add_boolean(token_nv, "compressok");
        }
        if (zap_contains(dp->dp_meta_objset, ds->ds_object,
                         DS_FIELD_RESUME_RAWOK) == 0)
        {
            fnvlist_add_boolean(token_nv, "rawok");
        }
        if (dsl_dataset_feature_is_active(ds,
                                          SPA_FEATURE_REDACTED_DATASETS))
        {
            uint64_t num_redact_snaps;
            uint64_t *redact_snaps;
            VERIFY(dsl_dataset_get_uint64_array_feature(ds,
                                                        SPA_FEATURE_REDACTED_DATASETS, &num_redact_snaps,
                                                        &redact_snaps));
            fnvlist_add_uint64_array(token_nv, "redact_snaps",
                                     redact_snaps, num_redact_snaps);
        }
        if (zap_contains(dp->dp_meta_objset, ds->ds_object,
                         DS_FIELD_RESUME_REDACT_BOOKMARK_SNAPS) == 0)
        {
            uint64_t num_redact_snaps, int_size;
            uint64_t *redact_snaps;
            VERIFY0(zap_length(dp->dp_meta_objset, ds->ds_object,
                               DS_FIELD_RESUME_REDACT_BOOKMARK_SNAPS, &int_size,
                               &num_redact_snaps));
            ASSERT3U(int_size, ==, sizeof(uint64_t));

            redact_snaps = kmem_alloc(int_size * num_redact_snaps,
                                      KM_SLEEP);
            VERIFY0(zap_lookup(dp->dp_meta_objset, ds->ds_object,
                               DS_FIELD_RESUME_REDACT_BOOKMARK_SNAPS, int_size,
                               num_redact_snaps, redact_snaps));
            fnvlist_add_uint64_array(token_nv, "book_redact_snaps",
                                     redact_snaps, num_redact_snaps);
            kmem_free(redact_snaps, int_size * num_redact_snaps);
        }
        packed = fnvlist_pack(token_nv, &packed_size);
        fnvlist_free(token_nv);
        compressed = kmem_alloc(packed_size, KM_SLEEP);

        compressed_size = gzip_compress(packed, compressed,
                                        packed_size, packed_size, 6);

        zio_cksum_t cksum;
        fletcher_4_native_varsize(compressed, compressed_size, &cksum);

        size_t alloc_size = compressed_size * 2 + 1;
        str = kmem_alloc(alloc_size, KM_SLEEP);
        for (int i = 0; i < compressed_size; i++)
        {
            size_t offset = i * 2;
            (void)snprintf(str + offset, alloc_size - offset,
                           "%02x", compressed[i]);
        }
        str[compressed_size * 2] = '\0';
        char *propval = kmem_asprintf("%u-%llx-%llx-%s",
                                      ZFS_SEND_RESUME_TOKEN_VERSION,
                                      (longlong_t)cksum.zc_word[0],
                                      (longlong_t)packed_size, str);
        kmem_free(packed, packed_size);
        kmem_free(str, alloc_size);
        kmem_free(compressed, packed_size);
        return (propval);
    }
    return (kmem_strdup(""));
}
*/