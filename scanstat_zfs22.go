//go:build zfs22

package zfs

/*
#include <libzfs.h>
#include "common.h"
#include "zpool.h"
*/
import "C"

// scanStatProgress reads the scan counters that differ between ZFS lines.
// See scanstat_zfs21.go for why this is split rather than renamed.
//
// 2.2 dropped pss_to_process entirely and placed pss_skipped where it used to
// be, so ToProcess has no source here and stays zero.
func scanStatProgress(ps C.pool_scan_stat_ptr) (toProcess, skipped uint64) {
	return 0, uint64(ps.pss_skipped)
}
