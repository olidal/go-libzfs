//go:build libzfs_source

package zfs
// #include <stdlib.h>
// #include <libzfs.h>
// #include <libzutil.h>
// #include <sys/zfs_ioctl.h>
// #include <zfs_fletcher.h>
// #include "common.h"
// #include "zpool.h"
// #include "zfs_stream.h"
import "C"

import (
	"errors"
	"unsafe"
)

const (
	DRR_BEGIN uint32 = iota
	DRR_OBJECT
	DRR_FREEOBJECTS
	DRR_WRITE
	DRR_FREE
	DRR_END
	DRR_WRITE_BYREF
	DRR_SPILL
	DRR_WRITE_EMBEDDED
	DRR_OBJECT_RANGE
	DRR_REDACT
	DRR_NUMTYPES
)

const (
	DRR_UNION_SIZE  = int(C.size_t(C.DRR_UNION_SIZE))
	DRR_STRUCT_SIZE = int(C.size_t(C.DRR_STRUCT_SIZE))
	MAXNAMELEN 		= int(C.size_t(C.MAXNAMELEN))
)

// All operation on dmu_replay_record are done on C side
// using Proxy go structures
// For convenience the Go top level proxy structure caches
// the type and payloadlen fields.
// Screenplay when receiving a stream on standard input
// 1. Read standard input in a DrrStructBytes byte array
// 2. call InitDrr() from DrrStructBytes to initialize a
// C struct dmu_replay_record and get the corresponding Go
// proxy struct


type DrrUnionItf interface {
	DrrBegin() (DrrBegin, error)
}
	

type DrrBegin struct {
	c_drr_begin_ptr *C.drr_begin_t
	DrrMagic 		uint64
	DrrVersioninfo 	uint64  /* was drr_version */
	DrrCreationTime uint64
	dmu_objset_type_t uint32
	DrrFlags 		uint32
	DrrToGUID 		uint64
	DrrFromGUID 	uint64
	DrrToName		[MAXNAMELEN]byte
}

type DrrEnd struct {
	_ [34]		uint64
	DrrChecksum C.zio_cksum_t
	DrrToGUID   uint64
}

type DmuReplayRecord struct {
	DrrType         uint32 // Cache/copy of C struct fields
	DrrPayloadLen   uint32 // idem
	DrrU 		    DrrUnionItf // methods/accessor interface
	// go and C mappings
	zcConfig        *ZfsStream
}

type ZfsStream struct {
	c_config	   *C.zfs_stream_config_ptr
	ZstreamItf interface {
		ZstreamReadDrr() (drr DmuReplayRecord, err error)
	}
}


func InitZfsStream(doCkSum bool) (zstream ZfsStream, err error) {
	zstream.c_config = C.init_stream(C.boolean_t(ToBoolean(doCkSum)),C.boolean_t(ToBoolean(false)),C.boolean_t(ToBoolean(true)))
	return
}

func (zstream *ZfsStream) Free() () {
	C.free(unsafe.Pointer(zstream.c_config))
}

func (zstream *ZfsStream) ZstreamReadDrr(fd int) (drr DmuReplayRecord, err error) {
	if zstream.c_config == nil {
		err = errors.New("Uninitialized ZfsStream!")
	} 
	nread := C.read_hdr(C.int(fd), zstream.c_config)
	if nread == C.size_t(0) {
		err = errors.New(C.GoString((*(zstream.c_config)).stream_error))
		return
	}

	drr.DrrType = uint32((*(zstream.c_config)).thedrr.drr_type)
	drr.DrrPayloadLen = uint32((*(zstream.c_config)).thedrr.drr_payloadlen)
	drr.zcConfig = zstream
	return
}

func (drr *DmuReplayRecord) DrrBegin() (drr_begin DrrBegin) {
	drr_begin.c_drr_begin_ptr = C.drr_get_begin(&((*(drr.zcConfig.c_config)).thedrr))
	return
}

func (drr_b *DrrBegin) Parse() {
	drr_b.DrrMagic = uint64((*drr_b.c_drr_begin_ptr).drr_magic)
	drr_b.DrrVersioninfo = uint64((*drr_b.c_drr_begin_ptr).drr_versioninfo)
}

