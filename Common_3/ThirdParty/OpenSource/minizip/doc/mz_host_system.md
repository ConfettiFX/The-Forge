# MZ_HOST_SYSTEM

Host system enumeration. These values correspond to section 4.4.2.2 of the [PKWARE zip app note](zip/appnote.txt).

|Name|Code|Description|
|-|-|-|
|MZ_HOST_SYSTEM_MSDOS|0|MS-DOS|
|MZ_HOST_SYSTEM_UNIX|3|UNIX|
|MZ_HOST_SYSTEM_WINDOWS_NTFS|10|Windows NTFS|
|MZ_HOST_SYSTEM_RISCOS|13|RISC OS|
|MZ_HOST_SYSTEM_OSX_DARWIN|19|Darwin|

 The host system information is available in the _version_madeby_ field in _mz_zip_file_.

**Example**
 ```
 mz_zip_file *file_info = NULL;
 mz_zip_entry_get_info(zip_handle, &file_info);
 int32_t host_sys = MZ_HOST_SYSTEM(file_info->version_madeby);
 printf("Host system value: %d\n", host_sys);
 if (host_sys == MZ_HOST_SYSTEM_MSDOS) {
     printf("Zip entry attributes are MS-DOS compatible\n");
 }
 ```