package zfs

/*
#include <stdlib.h>
#include <libzfs.h>
#include <libzutil.h>
#include "common.h"
#include "zpool.h"
#include "zfs.h"
*/
import "C"

import (
	"errors"
	"unsafe"
)

// GetPropertyFmt reads property p with configurable literal/humanized formatting.
// literal=false produces the same value that `zfs list` prints (e.g. "1G");
// literal=true returns the raw value (e.g. "1073741824").
// Equivalent to zfs_prop_get's literal argument.
func (d *Dataset) GetPropertyFmt(p Prop, literal bool) (prop Property, err error) {
	Global.Mtx.Lock()
	defer Global.Mtx.Unlock()
	if d.list == nil {
		err = errors.New(msgDatasetIsNil)
		return
	}
	lit := C.int(0)
	if literal {
		lit = 1
	}
	plist := C.read_dataset_property_fmt(d.list, C.int(p), lit)
	if plist == nil {
		err = LastError()
		return
	}
	defer C.free_properties(plist)
	prop = Property{
		Value:  C.GoString(&plist.value[0]),
		Source: C.GoString(&plist.source[0]),
	}
	return
}

// PropColumnName returns the short column header used by `zfs list`
// (e.g. "AVAIL" for ZFS_PROP_AVAILABLE, "REFER" for ZFS_PROP_REFERENCED).
// Returns "" for user-defined / unknown props.
func PropColumnName(p Prop) string {
	c := C.go_zfs_prop_column_name(C.int(p))
	if c == nil {
		return ""
	}
	return C.GoString(c)
}

// PropAlignRight reports whether a property is right-aligned in `zfs list`
// (typically numeric / size properties).
func PropAlignRight(p Prop) bool {
	return C.go_zfs_prop_align_right(C.int(p)) != 0
}

// NameToProp resolves a property name string to its Prop enum value.
// Returns (_, false) for user-defined property names or unknown names.
func NameToProp(name string) (Prop, bool) {
	c := C.CString(name)
	defer C.free(unsafe.Pointer(c))
	v := int(C.go_zfs_name_to_prop(c))
	if v < 0 {
		return 0, false
	}
	return Prop(v), true
}
