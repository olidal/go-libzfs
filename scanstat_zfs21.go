//go:build !zfs22

package zfs

/*
#include <libzfs.h>
#include "common.h"
#include "zpool.h"
*/
import "C"

// scanStatProgress reads the scan counters that differ between ZFS lines.
//
// 2.1 has pss_to_process, "total bytes to process". 2.2 removed it and put
// pss_skipped at the same offset, which is a different quantity -- so this is
// split per line rather than renamed. A binary built against the wrong headers
// would read one as the other with no error, and that is precisely the failure
// the split exists to make impossible: without the matching tag the build does
// not compile at all.
func scanStatProgress(ps C.pool_scan_stat_ptr) (toProcess, skipped uint64) {
	return uint64(ps.pss_to_process), 0
}
