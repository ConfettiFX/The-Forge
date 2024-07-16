set(BUNY_ARCHIVE_SRC
    Buny.c
    ../../Utilities/ThirdParty/OpenSource/lz4/lz4hc.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/fse_compress.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/hist.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/huf_compress.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/zstd_compress.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/zstd_compress_literals.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/zstd_compress_sequences.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/zstd_compress_superblock.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/zstd_double_fast.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/zstd_fast.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/zstd_lazy.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/zstd_ldm.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/zstdmt_compress.c
    ../../Utilities/ThirdParty/OpenSource/zstd/compress/zstd_opt.c
)

add_library(Buny STATIC ${BUNY_ARCHIVE_SRC})
