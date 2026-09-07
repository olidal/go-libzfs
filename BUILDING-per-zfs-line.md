# Building per OpenZFS line

`gozfsd` links libzfs, and OpenZFS 2.1 and 2.2 are not source-compatible. Two
places differ; both are selected by the **`zfs22` build tag**.

```bash
go build              ./cmd/gozfsd    # OpenZFS 2.1 (the default)
go build -tags zfs22  ./cmd/gozfsd    # OpenZFS 2.2
```

The default is 2.1 so existing build commands keep their meaning.

## Why a build tag, and not detection

There is no discriminator to detect: `_LIBZUTIL_H` is defined on both lines,
and the public headers expose no version macro. Checked on 2.1.15-pve1 and
2.2.10-pve1.

## What differs

| | 2.1 | 2.2 |
| --- | --- | --- |
| `zpool_search_import()` | `(void *, importargs_t *, const pool_config_ops_t *)` | `(libpc_handle_t *, importargs_t *)` — ops moved into the handle, which 2.2 publishes |
| `pool_scan_stat_t` | `pss_to_process` | `pss_skipped` **at the same offset**, a different quantity |

Only the differing call is per line — `zpool_searchimport_zfs2{1,2}.c` and
`scanstat_zfs2{1,2}.go`. The surrounding logic stays in one place.

`ScanStat` keeps one shape on both lines: `ToProcess` is filled on 2.1 and zero
on 2.2, `Skipped` the reverse. Neither is a real zero — the other line simply
has no such counter.

## The tag is a safety mechanism, not a convenience

**The SONAME does not protect you.** Both lines expose `libzfs.so.4`,
`libzfs_core.so.3` and `libnvpair.so.3`, so a binary built against 2.1 headers
*loads and runs* on a 2.2 host — measured, not assumed. It then reads
`pss_skipped` as `pss_to_process`: no error, wrong number.

Building with the wrong tag cannot produce that binary, because it does not
compile. Verified in all four combinations:

| host | tag | result |
| --- | --- | --- |
| 2.1.15 | none | builds |
| 2.1.15 | `zfs22` | **fails** — `unknown type name 'libpc_handle_t'` |
| 2.2.10 | `zfs22` | builds |
| 2.2.10 | none | **fails** — `too many arguments to function 'zpool_search_import'` |

That moves the failure from silent-at-runtime to loud-at-build-time. It does not
help a binary that is *shipped* to the wrong host, which stays a packaging
concern: one `gozfsd` per line, chosen at install from the version detected on
the target.
