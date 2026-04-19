//go:build libzfs_source

package zfs_test

import (
	"fmt"
	"os"
	"testing"

	zfs "github.com/olidal/go-libzfs"
)

func zfsTestStreamParseHdr(t *testing.T) {
	println("TEST Stream Parse HDR(", TSTDatasetPathSnap, ") ... ")
	d, err := zfs.DatasetOpen(TSTDatasetPathSnap)
	if err != nil {
		t.Error(err)
		return
	}
	defer d.Close()

	var err_send error
	r, w, err := os.Pipe()
	if err != nil {
		fmt.Println("Error creating pipe:", err)
		return
	}
	defer r.Close()
	defer w.Close()

	go func() {
		err_send = d.Send(w, zfs.SendFlags{Compress: true})
		w.Close()
	}()

	var zstream zfs.ZfsStream
	if zstream, err = zfs.InitZfsStream(true); err != nil {
		t.Error(err)
		return
	}
	defer (&zstream).Free()

	var drr zfs.DmuReplayRecord
	if drr, err = (&zstream).ZstreamReadDrr(int(r.Fd())); err != nil {
		t.Error(err)
		return
	}

	drr_b := (&drr).DrrBegin()
	drr_b.Parse()

	if err_send != nil {
		t.Error(err_send)
		return
	}
	fmt.Printf("Read Begin Record:\n Magic = %v, version=%v\n", drr_b.DrrMagic, drr_b.DrrVersioninfo)
	print("PASS\n\n")
}

func zfsTestForgeResumeToken(t *testing.T) {
	println("TEST ForgeResumeToken(1,2,3,4,5,'TOTO/titi',true,false,true,false)")
	var resToken zfs.ResumeToken
	token := zfs.ForgeReceiveResumeToken(1, 2, 3, 4, 5, "TOTO/titi", true, false, true, false)
	err := resToken.Unpack(token)
	if err != nil {
		t.Error(err)
		return
	}
	println(token)
	fmt.Printf("guid:   %v\nobject: %v\ntoname: %v\n",
		resToken.FromGUID, resToken.Object, resToken.ToName)

	switch {
	case resToken.FromGUID != 1:
		t.Fatalf("FromGUID mismatch: %v", resToken.FromGUID)
	case resToken.Object != 2:
		t.Fatalf("Object mismatch: %v", resToken.Object)
	case resToken.Offset != 3:
		t.Fatalf("Offset mismatch: %v", resToken.Offset)
	case resToken.Bytes != 4:
		t.Fatalf("Bytes mismatch: %v", resToken.Bytes)
	case resToken.ToGUID != 5:
		t.Fatalf("ToGUID mismatch: %v", resToken.ToGUID)
	case resToken.ToName != "TOTO/titi":
		t.Fatalf("ToName mismatch: %v", resToken.ToName)
	case resToken.LargeBlock != true:
		t.Fatalf("LargeBlock mismatch: %v", resToken.LargeBlock)
	case resToken.EmbedOk != false:
		t.Fatalf("EmbedOk mismatch: %v", resToken.EmbedOk)
	case resToken.CompressOk != true:
		t.Fatalf("CompressOk mismatch: %v", resToken.CompressOk)
	case resToken.RawOk != false:
		t.Fatalf("RawOk mismatch: %v", resToken.RawOk)
	}
	print("PASS\n\n")
}

func runLibzfsSourceTests(t *testing.T) {
	zfsTestStreamParseHdr(t)
	zfsTestForgeResumeToken(t)
}
