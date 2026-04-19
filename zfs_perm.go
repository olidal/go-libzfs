package zfs

/*
#include <stdlib.h>
#include <libzfs.h>
#include <libzutil.h>
#include "common.h"
#include "zpool.h"
#include "zfs.h"
#include "zfs_perm.h"
*/
import "C"

import (
	"fmt"
	"strings"
	"unsafe"
)

// DelegLocality is a bitmask of delegation scope.
type DelegLocality int

const (
	// DelegLocal applies only to the dataset itself.
	DelegLocal DelegLocality = C.GO_PERM_LOCAL
	// DelegDescendent applies to descendent datasets.
	DelegDescendent DelegLocality = C.GO_PERM_DESCENDENT
	// DelegLocalDescendent applies to the dataset and all descendents
	// (the default for `zfs allow -u`).
	DelegLocalDescendent = DelegLocal | DelegDescendent
)

// DelegAllowEntry is a single delegation: a user with a set of permissions
// at a given scope.
type DelegAllowEntry struct {
	User     string
	Perms    []string
	Locality DelegLocality
}

// Allow grants permissions to a user on this dataset.
// who may be a username or a numeric uid as a string.
// Equivalent to: zfs allow -u <who> <perms> <dataset>
func (d *Dataset) Allow(who string, perms []string, locality DelegLocality) error {
	return d.setAllow(who, perms, locality, false)
}

// Unallow revokes permissions from a user on this dataset.
// Equivalent to: zfs unallow -u <who> <perms> <dataset>
func (d *Dataset) Unallow(who string, perms []string, locality DelegLocality) error {
	return d.setAllow(who, perms, locality, true)
}

func (d *Dataset) setAllow(who string, perms []string, locality DelegLocality, unset bool) error {
	if d.list == nil {
		return fmt.Errorf("dataset is closed")
	}
	cwho := C.CString(who)
	defer C.free(unsafe.Pointer(cwho))

	cperms := C.CString(strings.Join(perms, ","))
	defer C.free(unsafe.Pointer(cperms))

	unsetInt := C.int(0)
	if unset {
		unsetInt = 1
	}

	rc := C.go_zfs_user_allow(d.list, cwho, cperms, C.int(locality), unsetInt)
	if rc != 0 {
		return fmt.Errorf("zfs allow/unallow: rc=%d", int(rc))
	}
	return nil
}

// GetAllow returns all user delegations on this dataset.
// Group, everyone, permission-set, and create-time entries are skipped.
// Each (user, locality) pair is a separate entry — so a user with both
// local and descendent grants appears twice.
func (d *Dataset) GetAllow() ([]DelegAllowEntry, error) {
	if d.list == nil {
		return nil, fmt.Errorf("dataset is closed")
	}
	var head *C.go_deleg_entry_t
	rc := C.go_zfs_get_allow(d.list, &head)
	if rc != 0 {
		return nil, fmt.Errorf("zfs_get_fsacl: rc=%d", int(rc))
	}
	defer C.go_zfs_free_deleg_entries(head)

	var entries []DelegAllowEntry
	for e := head; e != nil; e = e.next {
		user := C.GoString(&e.user[0])
		permsStr := C.GoString(&e.perms[0])
		var perms []string
		if permsStr != "" {
			perms = strings.Split(permsStr, ",")
		}
		entries = append(entries, DelegAllowEntry{
			User:     user,
			Perms:    perms,
			Locality: DelegLocality(e.locality),
		})
	}
	return entries, nil
}
