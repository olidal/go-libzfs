//go:build libzfs_source

package zfs

// #include <stdlib.h>
// #include <libzfs.h>
// #include <libzutil.h>
// #include "common.h"
// #include "zpool.h"
// #include "zfs_token.h"
import "C"

import (
	"unsafe"
)

func ForgeReceiveResumeToken(
    fromguid 		uint64,
    object   		uint64,
    offset   		uint64,
    bytes    		uint64,
    toguid   		uint64,
    toname   		string,
    largeblockok 	bool,
    embedok 		bool,
    compressok      bool,
	rawok			bool) (token string) {
	ctoken := C.forge_receive_resume_token(
			C.uint64_t(fromguid),
			C.uint64_t(object),
			C.uint64_t(offset),
			C.uint64_t(bytes),
			C.uint64_t(toguid),
			C.CString(toname),
			ToBoolean(largeblockok),
			ToBoolean(embedok),
			ToBoolean(compressok),
			ToBoolean(rawok))
	defer C.free(unsafe.Pointer(ctoken))
	token = C.GoString(ctoken)
	return
}
