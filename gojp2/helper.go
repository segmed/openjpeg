package gojp2

import (
	"fmt"
	"image"
	"image/color"
	"math"
	"os"
	"unsafe"

	"modernc.org/libc"
)

func Xfmin(tls *libc.TLS, a, b float64) float64 {
	return math.Min(a, b)
}

func Xlrintf(tls *libc.TLS, a float32) int64 {
	return int64(math.Round(float64(a)))
}

// int fscanf(FILE *stream, const char *format, ...);
func Xfscanf(t *libc.TLS, stream, format uintptr, args ...interface{}) int32 {
	file := os.NewFile(stream, "")
	stringPtr := (*string)(unsafe.Pointer(format))
	n, err := fmt.Fscanf(file, *stringPtr, args)
	if err != nil {
		return -1
	}
	return int32(n)
}

// GoImageToNativeData coverts GO native image to DICOM native image
func GoImageToNativeData(img image.Image, bitsPerSample, samples, rows, cols int) [][]int { /* helper.c:156:5: */
	var data [][]int
	for i := 0; i < rows; i++ {
		for j := 0; j < cols; j++ {
			c := img.At(i, j)
			switch samples {
			case 3:
				r, g, b, _ := c.RGBA()
				data = append(data, []int{int(r), int(g), int(b)})
			case 1:
				var y int
				if bitsPerSample > 8 {
					y = int(color.Gray16Model.Convert(c).(color.Gray16).Y)
				} else {
					y = int(color.GrayModel.Convert(c).(color.Gray).Y)
				}
				data = append(data, []int{int(y)})
			}
		}
	}
	return data
}

// DecodeJ2KImage converts JPEG2000 bytes to DICOM Native Data
func DecodeJ2KImage(data []byte) (int, int, [][]int, error) {
	tls := libc.NewTLS()
	defer tls.Close()
	if data == nil {
		return 0, 0, nil, fmt.Errorf("data is empty")
	}
	var opjImage uintptr = Xdecode_j2k(tls, uintptr(unsafe.Pointer(&data[0])), uint64(len(data)))
	if (*Topj_image_t)(unsafe.Pointer(opjImage)) == nil {
		return 0, 0, nil, fmt.Errorf("openjpeg: failed to decode image")
	}
	w, h, out := XopjImageToNativeData((*Topj_image_t)(unsafe.Pointer(opjImage)))
	return w, h, out, nil
}

// XopjImagetoDicomImage converts OPJ_IMAGE to DICOM Native Data
func XopjImageToNativeData(opjImage *Topj_image_t) (int, int, [][]int) {
	var i, j, c int
	var compno uint32
	var ncomp = (*Topj_image_t)(unsafe.Pointer(opjImage)).Fnumcomps
	var exComp Sopj_image_comp
	var exData TOPJ_INT32
	var sizeOfFcomps = unsafe.Sizeof(exComp)
	var sizeOfData = unsafe.Sizeof(exData)
	var nativeData [][]int

	wr := int((*Topj_image_comp_t)(unsafe.Pointer((*Topj_image_t)(unsafe.Pointer(opjImage)).Fcomps)).Fw)
	hr := int((*Topj_image_comp_t)(unsafe.Pointer((*Topj_image_t)(unsafe.Pointer(opjImage)).Fcomps)).Fh)

	for i = 0; i < hr; i++ {
		for j = 0; j < wr; j++ {
			var vals [4]int32
			for compno = uint32(0); compno < ncomp; compno++ {
				vals[compno] = *(*TOPJ_INT32)(unsafe.Pointer((*Topj_image_comp_t)(unsafe.Pointer((*Topj_image_t)(unsafe.Pointer(opjImage)).Fcomps+uintptr(compno)*sizeOfFcomps)).Fdata + uintptr(c)*sizeOfData))
			}
			r, g, b, a := vals[0], vals[1], vals[2], vals[3]
			switch ncomp {
			case 1:
				nativeData = append(nativeData, []int{int(r)})
			case 3:
				nativeData = append(nativeData, []int{int(r), int(g), int(b)})
			case 4:
				nativeData = append(nativeData, []int{int(r), int(g), int(b), int(a)})
			}
			c++
		}
	}
	return wr, hr, nativeData
}

// XopjImagetoGoImage converts OPJ_IMAGE to GO native image
func XopjImagetoGoImage(opjImage *Topj_image_t) image.Image { /* helper.c:282:5: */
	var i, j, c int32
	var compno uint32
	var ncomp = (*Topj_image_t)(unsafe.Pointer(opjImage)).Fnumcomps
	var exComp Sopj_image_comp
	var exData TOPJ_INT32
	var sizeOfFcomps = unsafe.Sizeof(exComp)
	var sizeOfData = unsafe.Sizeof(exData)

	wr := int32((*Topj_image_comp_t)(unsafe.Pointer((*Topj_image_t)(unsafe.Pointer(opjImage)).Fcomps)).Fw)
	hr := int32((*Topj_image_comp_t)(unsafe.Pointer((*Topj_image_t)(unsafe.Pointer(opjImage)).Fcomps)).Fh)
	upLeft := image.Point{0, 0}
	lowRight := image.Point{int(hr), int(wr)}
	img16 := image.NewRGBA64(image.Rectangle{upLeft, lowRight})

	for i = 0; i < hr; i++ {
		for j = 0; j < wr; j++ {
			var vals [4]int32
			for compno = uint32(0); compno < ncomp; compno++ {
				vals[compno] = *(*TOPJ_INT32)(unsafe.Pointer((*Topj_image_comp_t)(unsafe.Pointer((*Topj_image_t)(unsafe.Pointer(opjImage)).Fcomps+uintptr(compno)*sizeOfFcomps)).Fdata + uintptr(c)*sizeOfData))
			}
			r, g, b, a := vals[0], vals[1], vals[2], vals[3]
			if ncomp == 1 {
				img16.Set(int(i), int(j), color.Gray16{uint16(r)})
			} else {
				img16.Set(int(i), int(j), color.RGBA64{uint16(r), uint16(g), uint16(b), uint16(a)})
			}
			c++
		}
	}
	return img16
}
