# MZ_ZIP64

Zip64 mode enumeration. The zip64 extension is documented in [PKWARE zip app note](zip/appnote.txt) section 4.5.3 and provides support for zip files and entries greater than 4GB. These modes are only supported while writing a zip entry.

|Name|Code|Description|
|-|-|-|
|MZ_ZIP64_AUTO|0|Only store and use zip64 extrafield if compressed size, uncompressed size, or disk offset is greater than -UINT32_MAX_.|
|MZ_ZIP64_FORCE|1|Always use and store zip64 extrafield even if it is not necessary.|
|MZ_ZIP64_DISABLE|2|Never use or store zip64 extrafield. If zip64 is required to write the entry it will result in MZ_PARAM_ERROR.|
