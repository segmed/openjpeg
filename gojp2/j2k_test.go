package gojp2

import (
	"os"
	"testing"
	"unsafe"

	"modernc.org/libc"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

type testify struct {
	in                   string
	out                  string
	w, h, prec, numcomps int32
}

func TestMonoDecompresing(t *testing.T) {
	for _, test := range []testify{
		{
			in:       "testdata/a1_mono.j2c",
			out:      "testdata/a1_mono.ppm",
			w:        303,
			h:        179,
			prec:     8,
			numcomps: 1,
		},
	} {
		t.Run("decompress "+test.in, func(t *testing.T) {
			assertJ2KDecompressing(t, test.in, test.out)
		})
	}
}

func assertJ2KDecompressing(t *testing.T, in, out string) {
	tls := libc.NewTLS()
	defer tls.Close()
	input, err := os.ReadFile(in)
	require.NoError(t, err)
	var opjImage uintptr = Xdecode_j2k(tls, uintptr(unsafe.Pointer(&input[0])), uint64(len(input)))
	if (*Topj_image_t)(unsafe.Pointer(opjImage)) == nil {
		t.Errorf("openjpeg: failed to decode image")
	}
	var ncomp = (*Topj_image_t)(unsafe.Pointer(opjImage)).Fnumcomps
	require.EqualValues(t, 1, ncomp) //mono image
	wr := int((*Topj_image_comp_t)(unsafe.Pointer((*Topj_image_t)(unsafe.Pointer(opjImage)).Fcomps)).Fw)
	hr := int((*Topj_image_comp_t)(unsafe.Pointer((*Topj_image_t)(unsafe.Pointer(opjImage)).Fcomps)).Fh)
	assert.NotZero(t, wr)
	assert.NotZero(t, hr)

	var exComp Sopj_image_comp
	var exData TOPJ_INT32
	var sizeOfFcomps = unsafe.Sizeof(exComp)
	var sizeOfData = unsafe.Sizeof(exData)

	var bytesData []byte
	for i := 0; i < hr*wr; i++ {
		var val = *(*TOPJ_INT32)(unsafe.Pointer((*Topj_image_comp_t)(unsafe.Pointer((*Topj_image_t)(unsafe.Pointer(opjImage)).Fcomps+uintptr(0)*sizeOfFcomps)).Fdata + uintptr(i)*sizeOfData))
		bytesData = append(bytesData, byte(val))
	}
	bytesData = append(bytesData, 0x0a)

	output, err := os.ReadFile(out)
	require.NoError(t, err)
	il := len(bytesData)
	ol := len(output)
	require.Less(t, il, ol)
	require.Equal(t, output[ol-il:ol], bytesData)
}
