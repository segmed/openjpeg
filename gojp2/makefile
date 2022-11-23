all: decompress decompress.darwin.arm64 decompress.linux

CMD=ccgo  -pkgname gojp2  -trace-translation-units -export-externs X -export-defines D -export-fields F -export-structs S -export-typedefs T -I../src/lib/openjp2/ -I../src/bin/common/ ../src/bin/common/color.c ../src/lib/openjp2/*.c

decompress:
	$(CMD)

decompress.linux:
	TARGET_GOARCH=amd64 TARGET_GOOS=linux $(CMD)
	TARGET_GOARCH=arm64 TARGET_GOOS=linux $(CMD)

decompress.darwin.arm64:
	TARGET_GOARCH=arm64 TARGET_GOOS=darwin $(CMD)
