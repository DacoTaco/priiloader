    .section .rodata
    .global loader
    .align  4
loader:
    .incbin "loader.bin"
loaderEnd:
    .global loaderSize
    .align  4
loaderSize:
    .int    loaderEnd - loader
