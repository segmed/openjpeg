## Transpile openjp2 to Go

### Convert bmp to JPEG2000
```
make compress
go run . -i ./image.bmp -o image.j2k
```

### Convert JPEG2000 to bmp
```
make decompress
go run . -i ./image.j2k -o image.bmp
```
