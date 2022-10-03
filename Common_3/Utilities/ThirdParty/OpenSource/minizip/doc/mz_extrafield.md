# Extrafields

These are proposals for additional extrafields that are not in the ZIP specification.

## Hash (0x1a51)

|Size|Type|Description|
|-|-|:-|
|2|uint16_t|Algorithm|
|2|uint16_t|Digest size|
|*|uint8_t*|Digest|

|Value|Description|
|-|:-|
|10|MD5|
|20|SHA1|
|23|SHA256|

By default, the ZIP specification only includes a CRC hash of the uncompressed content. The Hash extrafield allows additional hash digests of the uncompressed content to be stored with the central directory or local file headers. If there are multiple Hash extrafields stored for an entry, they should be sorted in order of most secure to least secure. So that the first Hash extrafield that appears for the entry is always the one considered the most secure and the one used for signing.

## CMS Signature (0x10c5)

|Size|Type|Description|
|-|-|:-|
|*|uint8_t*|Variable-length signature|

Stores a CMS signature whose message is a hash of the uncompressed content of the entry. The hash must correspond to the first Hash (0x1a51) extrafield stored in the entry's extrafield list.

## Central Directory (0xcdcd)

|Size|Type|Description|
|-|-|:-|
|8|uint64_t|Number of entries|

If the zip entry is the central directory for the archive, then this record contains information about that central directory.
