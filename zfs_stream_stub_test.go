//go:build !libzfs_source

package zfs_test

import "testing"

// runLibzfsSourceTests is a no-op when built without -tags libzfs_source.
// The real implementation in zfs_stream_test.go exercises DRR header parsing
// and resume-token forging via libzfs private headers that are not available
// in distro packages.
func runLibzfsSourceTests(t *testing.T) {}
