package gojp2

import (
	"fmt"
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
