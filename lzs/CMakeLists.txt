file(GLOB headers "*.h")
file(GLOB source "*.c")

add_library(lzs STATIC ${source} ${headers})
