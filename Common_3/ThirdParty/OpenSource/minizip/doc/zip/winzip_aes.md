
# AES Encryption Information:
Encryption Specification AE-1 and AE-2

**Document version: 1.04
Last modified: January 30, 2009**

**NOTE: WinZip®  users do not need to read or understand the information contained on this page. It is intended for developers of Zip file utilities.**

Changes since the original version of this document are summarized in the  [Change History](https://www.winzip.com/aes_info.htm#changes)  section below.

This document describes the file format that WinZip uses to create  [AES-encrypted Zip files](https://www.winzip.com/learn/aes-encryption.html). The AE-1 encryption specification was introduced in WinZip 9.0 Beta 1, released in May 2003. The AE-2 encryption specification, a minor variant of the original AE-1 specification differing only in how the CRC is handled, was introduced in WinZip 9.0 Beta 3, released in January, 2004. Note that as of WinZip 11, WinZip itself encrypts most files using the AE-1 format and encrypts others using the AE-2 format.

From time to time we may update the information provided here, for example to document any changes to the file formats, or to add additional notes or implementation tips. If you would like to receive e-mail announcements of any substantive changes we make to this document, you can  [sign up below](https://www.winzip.com/aes_info.htm#mailing-list)for our Developer Information mailing list.

Without compromising the basic Zip file format, WinZip Computing has extended the format specification to support AES encryption, and this document fully describes the format extension. Additionally, we are providing information about a no-cost third-party source for the actual AES encryption code--the same code that is used by WinZip. We believe that use of the free encryption code and of this specification will make it easy for all developers to add compatible advanced encryption to their Zip file utilities.

This document is not a tutorial on encryption or Zip file structure. While we have attempted to provide the necessary details of the current WinZip AES encryption format, developers and other interested third parties will need to have or obtain an understanding of basic encryption concepts, Zip file format, etc.

Developers should also review  [AES Coding Tips](https://www.winzip.com/aes_tips.html)  page.

WinZip Computing makes no warranties regarding the information provided in this document. In particular, WinZip Computing does not represent or warrant that the information provided here is free from errors or is suitable for any particular use, or that the file formats described here will be supported in future versions of WinZip. You should test and validate all code and techniques in accordance with good programming practice.

----------

**I. Encryption services**

To perform AES encryption and decryption, WinZip uses AES functions written by Dr. Brian Gladman. The source code for these functions is available in C/C++ and Pentium family assembler for anyone to use under an open source BSD or GPL license from  [the AES project page](https://www.winzip.com/gladman.cgi)  on Dr. Gladman's web site. The  [AES Coding Tips](https://www.winzip.com/aes_tips.html)  page also has some information on the use of these functions. WinZip Computing thanks Dr. Gladman for making his AES functions available to anyone under liberal license terms.

Dr. Gladman's encryption functions are portable to a number of operating systems and can be static linked into your applications, so there are no operating system version or library dependencies. In particular, the functions do not require Microsoft's Cryptography API.

General information on the AES standard and the encryption algorithm (also known as _Rijndael_) is readily available on the Internet. A good place to start is  [https://web.archive.org/web/20100407101917/www.nist.gov/public_affairs/releases/g00-176.htm](https://web.archive.org/web/20100407101917/www.nist.gov/public_affairs/releases/g00-176.htm).

**II. Zip file format**

1.  Base format reference

    AES-encrypted files are stored within the guidelines of the standard Zip file format using only a new "extra data" field, a new compression method code, and a value in the CRC field dependant on the encryption version, AE-1 or AE-2. The basic Zip file format is otherwise unchanged.

    WinZip sets the version needed to extract and version made by fields in the local and central headers to the same values it would use if the file were not encrypted.

    The basic Zip file format specification used by WinZip is available via FTP from the Info-ZIP group at  [https://infozip.sourceforge.io/doc/appnote-iz-latest.zip](https://infozip.sourceforge.io/doc/appnote-iz-latest.zip).

2.  Compression method and encryption flag

    As for any encrypted file, bit 0 of the "general purpose bit flags" field must be set to 1 in each AES-encrypted file's local header and central directory entry.

    Additionally, the presence of an AES-encrypted file in a Zip file is indicated by a new compression method code (decimal 99) in the file's local header and central directory entry, used along with the AES extra data field described below. There is no change in either the  _version made by_  or  _version needed to extract_  codes.

    The code for the actual compression method is stored in the AES extra data field (see below).

3.  CRC value

    For files encrypted using the AE-2 method, the standard Zip CRC value is  _not_  used, and a 0 must be stored in this field. Corruption of encrypted data within a Zip file is instead detected via the  [authentication code](https://www.winzip.com/aes_info.htm#authentication-code)  field.

    Files encrypted using the AE-1 method  _do_  include the standard Zip CRC value. This, along with the fact that the vendor version stored in the AES extra data field is 0x0001 for AE-1 and 0x0002 for AE-2, is the only difference between the AE-1 and AE-2 formats.

    **NOTE:**  Zip utilities that support the AE-2 format are required to be able to read files that were created in the AE-1 format, and during decryption/extraction of files in AE-1 format should verify that the file's CRC matches the value stored in the CRC field.

4.  AES extra data field
    1.  A file encrypted with AES encryption will have a special "extra data" field associated with it. This extra data field is stored in both the local header and central directory entry for the file.

        Note: see the Zip file format document referenced above for general information on the format and use of extra data fields.

    2.  The extra data header ID for AES encryption is 0x9901. The fields are all stored in Intel low-byte/high-byte order. The extra data field currently has a length of 11: seven data bytes plus two bytes for the header ID and two bytes for the data size. Therefore, the extra data overhead for each file in the archive is 22 bytes (11 bytes in the central header plus 11 bytes in the local header).
    3.  The format of the data in the AES extra data field is as follows. See the notes below for additional information.

        |**Offset**|**Size(bytes)**|**Content**|
        |:-|:-|:-|
        |0|2|Extra field header ID (0x9901)|
        |2|2|Data size (currently 7, but subject to possible increase in the future)|
        |4|2|Integer version number specific to the zip vendor|
        |6|2|2-character vendor ID|
        |8|1|Integer mode value indicating AES encryption strength|
        |9|2|The actual compression method used to compress the file|

    4.  Notes
        -   **Data size**: this value is currently 7, but because it is possible that this specification will be modified in the future to store additional data in this extra field, vendors should not assume that it will always remain 7.
        -   **Vendor ID**: the vendor ID field should always be set to the two ASCII characters "AE".
        -   **Vendor version**: the vendor version for AE-1 is 0x0001. The vendor version for AE-2 is 0x0002.

            Zip utilities that support AE-2 must also be able to process files that are encrypted in AE-1 format. The handling of the  [CRC value](https://www.winzip.com/aes_info.htm#CRC)  is the only difference between the AE-1 and AE-2 formats.

        -   **Encryption strength**: the mode values (encryption strength) for AE-1 and AE-2 are:

            |**Value**|**Strength**|
            |-|-|
            |0x01|128-bit encryption key|
            |0x02|192-bit encryption key|
            |0x03|256-bit encryption key|

            The encryption specification supports  _only_  128-, 192-, and 256-bit encryption keys. No other key lengths are permitted.

            (Note: the current version of WinZip does not support encrypting files using 192-bit keys. This specification, however, does provide for the use of 192-bit keys, and WinZip is able to decrypt such files.)

        -   **Compression method**: the compression method is the one that would otherwise have been stored in the local and central headers for the file. For example, if the file is imploded, this field will contain the compression code 6. This is needed because a compression method of 99 is used to indicate the presence of an AES-encrypted file (see  [above](https://www.winzip.com/aes_info.htm#comp-method)).

**III. Encrypted file storage format**

1.  File format

    Additional overhead data required for decryption is stored with the encrypted file itself (i.e., not in the headers). The actual format of the stored file is as follows; additional information about these fields is below. All fields are byte-aligned.

    |**Size  (bytes)**|**Content**|
    |:-|:-|
    |Variable|Salt value|
    |2|Password verification value|
    |Variable|Encrypted file data|
    |10|Authentication code|

    Note that the value in the "compressed size" fields of the local file header and the central directory entry is the total size of all the items listed above. In other words, it is the total size of the salt value, password verification value, encrypted data, and authentication code.

2.  Salt value

    The "salt" or "salt value" is a random or pseudo-random sequence of bytes that is combined with the encryption password to create encryption and authentication keys. The salt is generated by the encrypting application and is stored unencrypted with the file data. The addition of salt values to passwords provides a number of security benefits and makes dictionary attacks based on precomputed keys much more difficult.

    Good cryptographic practice requires that a different salt value be used for each of multiple files encrypted with the same password. If two files are encrypted with the same password and salt, they can leak information about each other. For example, it is possible to determine whether two files encrypted with the same password and salt are identical, and an attacker who somehow already knows the contents of one of two files encrypted with the same password and salt can determine some or all of the contents of the other file. Therefore, you should make every effort to use a unique salt value for each file.

    The size of the salt value depends on the length of the encryption key, as follows:

    |**Key size**|**Salt size**|
    |:-|:-|
    |128 bits|8 bytes|
    |192 bits|12 bytes|
    |256 bits|16 bytes|

3.  Password verification value

    This two-byte value is produced as part of the process that derives the encryption and decryption keys from the password. When encrypting, a verification value is derived from the encryption password and stored with the encrypted file. Before decrypting, a verification value can be derived from the decryption password and compared to the value stored with the file, serving as a quick check that will detect  _most_, but not all, incorrect passwords. There is a 1 in 65,536 chance that an incorrect password will yield a matching verification value; therefore, a matching verification value cannot be absolutely relied on to indicate a correct password.

    Information on how to obtain the password verification value from Dr. Gladman's encryption library can be found on the  [coding tips](https://www.winzip.com/aes_tips.html)  page.

    This value is stored unencrypted.

4.  Encrypted file data

    Encryption is applied only to the content of files. It is performed after compression, and not to any other associated data. The file data is encrypted byte-for-byte using the AES encryption algorithm operating in "CTR" mode, which means that the lengths of the compressed data and the compressed, encrypted data are the same.

    It is important for implementors to note that, although the data is encrypted byte-for-byte, it is presented to the encryption and decryption functions in blocks. The block size used for encryption and decryption must be the same. To be compatible with the encryption specification, this block size must be 16 bytes (although the last block may be smaller).

5.  Authentication code

    Authentication provides a high quality check that the contents of an encrypted file have not been inadvertently changed or deliberately tampered with since they were first encrypted. In effect, this is a super-CRC check on the data in the file  _after_  compression and encryption. (Additionally, authentication is essential when using CTR mode encryption because this mode is vulnerable to several trivial attacks in its absence.)

    The authentication code is derived from the output of the encryption process.  [Dr. Gladman's AES code](https://www.winzip.com/aes_info.htm#encryption)  provides this service, and information about how to obtain it is in the  [coding tips](https://www.winzip.com/aes_tips.html).

    The authentication code is stored unencrypted. It is byte-aligned and immediately follows the last byte of encrypted data.

    For more discussion about authentication, see the  [authentication code FAQ](https://www.winzip.com/aes_info.htm#auth-faq)  below.


**IV. Changes in WinZip 11**

Beginning with WinZip 11, WinZip makes a change in its use of the AE-1 and AE-2 file formats. The file formats themselves have not changed, and AES-encrypted files created by WinZip 11 are completely compatible with version 1.02 the WinZip AES encryption specification, which was published in January 2004.

[WinZip 9.0](https://www.winzip.com/win/en/winzip-9.html)  and  [WinZip 10.0](https://www.winzip.com/win/en/winzip-10.html)  stored all AES-encrypted files using the AE-2 file format, which does not store the encrypted file's CRC. WinZip 11 instead uses the AE-1 file format, which does store the CRC, for most files. This provides an extra integrity check against the possibility of hardware or software errors that occur during the actual process of  [file compression/encryption](https://www.winzip.com/win/en/features/data-protection.html)  or decryption/decompression. For more information on this point, see the discussion of the  [CRC](https://www.winzip.com/aes_info.htm#crc-faq)  below.

Because for some very small files the CRC can be used to determine the exact contents of a file, regardless of the encryption method used, WinZip 11 continues to use the AE-2 file format, with no CRC stored, for files with an uncompressed size of less than 20 bytes. WinZip 11 also uses the AE-2 file format for files compressed in BZIP2 format, because the BZIP2 format contains its own integrity checks equivalent to those provided by the Zip format's CRC.

Other vendors who support WinZip's AES encryption specification may want to consider making a similar change to their own implementations of the specification, to get the benefit of the extra integrity check that it provides.

Note that the January 2004 version of the WinZip AE-2 specification, version 1.0.2, already required that all utilities that implemented the AE-2 format also be able to process files in AE-1 format, and should check on decryption/extraction of those files that the CRC was correct.

**V. Notes**

1.  Non-files and zero-length files

    To reduce Zip file size, it is recommended that non-file entries such as folder/directory entries not be encrypted. This, however, is only a recommendation; it is permissible to encrypt or not encrypt these entries, as you prefer.

    On the other hand, it is recommended that you  _do_  encrypt zero-length files. The presence of both encrypted and unencrypted files in a Zip file may trigger user warnings in some Zip file utilities, so the user experience may be improved if all files (including zero-length files) are encrypted.

    If zero-length files are encrypted, the encrypted data portion of the file storage (see  [above](https://www.winzip.com/aes_info.htm#encrypted-data)) will be empty, but the remainder of the encryption overhead data must be present, both in the file storage area and in the local and central headers.

2.  "Mixed" Zip files

    There is no requirement that all files in a Zip file be encrypted or that all files that  _are_  encrypted use the same encryption method or the same password.

    A Zip file can contain any combination of unencrypted files and files encrypted with any of the four currently defined encryption methods (Zip 2.0, AES-128, AES-192, AES-256). Encrypted files may use the same password or different passwords.

3.  Key Generation

    Key derivation, as used by AE-1 and AE-2 and as implemented in Dr. Gladman's library, is done according to the PBKDF2 algorithm, which is described in the  [RFC2898](https://www.ietf.org/rfc/rfc2898.txt)  guidelines. An iteration count of 1000 is used. An appropriate number of bits from the resulting hash value are used to compose three output values: an encryption key, an authentication key, and a password verification value. The first  _n_  bits become the encryption key, the next  _m_  bits become the authentication key, and the last 16 bits (two bytes) become the password verification value.

    As part of the process outlined in RFC 2898 a pseudo-random function must be called; AE-2 uses the HMAC-SHA1 function, since it is a well-respected algorithm that has been in wide use for this purpose for several years.

    Note that, when used in connection with 192- or 256-bit AES encryption, the fact that HMAC-SHA1 produces a 160-bit result means that, regardless of the password that you specify, the search space for the encryption key is unlikely to reach the theoretical 192- or 256-bit maximum, and cannot be guaranteed to exceed 160 bits. This is discussed in section B.1.1 of the  [RFC2898 specification](https://www.ietf.org/rfc/rfc2898.txt).


**VI. FAQs**

-   **Why is the compression method field used to indicate AES encryption?**

    As opposed to using new  _version made by_  and  _version needed to extract_  values to signal AES encryption for a file, the new compression method is more likely to be handled gracefully by older versions of existing Zip file utilities. Zip file utilities typically do not attempt to extract files compressed with unknown methods, presumably notifying the user with an appropriate message.

-   **How can I guarantee that the salt value is unique?**

    In principle, the value of the salt should be different whenever the same password is used more than once, for the reasons described  [above](https://www.winzip.com/aes_info.htm#salt), but this is difficult to guarantee.

    In practice, the number of bytes in the salt (as specified by AE-1 and AE-2) is such that using a pseudo-random value will ensure that the probability of duplicated salt values is very low and can be safely ignored.

    There is one exception to this: With the 8-byte salt values used with WinZip's 128-bit encryption it is likely that, if approximately 4 billion files are encrypted with the same password, two of the files will have the same salt, so it is advisable to stay well below this limit. Because of this, when using the same password to encrypt very large numbers of files in WinZip's AES encryption format (that is, files totalling in the millions, for example 2000 Zip files, each containing 1000 encrypted files), we recommend the use of 192-bit or 256-bit AES keys, with their 12- and 16-byte salt values, rather than 128-bit AES keys, with their 8-byte salt values.

    Although salt values do not need to be truly random, it is important that they be generated in a way that the probability of duplicated salt values is not significantly higher than that which would be expected if truly random values were being used.

    One technique for generating salt values is presented in the  [coding tips](https://www.winzip.com/aes_tips.html#prng)  page.

-   **Why is there an authentication code?**

    The purpose of the authentication code is to insure that, once a file's data has been compressed and encrypted, any accidental corruption of the encrypted data, and any deliberate attempts to modify the encrypted data by an attacker who does not know the password, can be detected.

    The current consensus in the cryptographic community is that associating a message authentication code (or MAC) with encrypted data has strong security value because it makes a number of attacks more difficult to engineer. For AES CTR mode encryption in particular, a MAC is especially important because a number of trivial attacks are possible in its absence. The MAC used with WinZip's AES encryption is based on[HMAC-SHA1-80](https://www.ietf.org/rfc/rfc2104.txt), a mature and widely respected authentication algorithm.

    The MAC is calculated after the file data has been compressed and encrypted. This order of calculation is referred to as Encrypt-then-MAC, and is preferred by many cryptographers to the alternative order of MAC-then-Encrypt because Encrypt-then-MAC is immune to some known attacks on MAC-then-Encrypt.

-   **What is the role of the CRC in WinZip 11?**

    Within the Zip format, the primary use of the CRC value is to detect accidental corruption of data that has been stored in the Zip file. With files encrypted according to the Zip 2.0 encryption specification, it also functions to some extent as a method of detecting deliberate attempts to modify the encrypted data, but not one that can be considered cryptographically strong. The CRC is not needed for these purposes with the WinZip AES encryption specification, where the HMAC-SHA1-based authentication code instead serves these roles.

    The CRC has a drawback in that for very small files, such as files with four or fewer bytes, the CRC can be used, independent of the encryption algorithm, to determine the unencrypted contents of the file. And, in general, it is preferable to store as little information as possible about the encrypted file in the unencrypted Zip headers.

    The CRC does serve one purpose that the authentication code does not. The CRC is computed based on the original uncompressed, unencrypted contents of the file, and it is checked after the file has been decrypted and decompressed. In contrast, the authentication code used with WinZip AES encryption is computed after compression/encryption and it is checked before decryption/decompression. In the very rare event of a hardware or software error that corrupts data during compression and encryption, or during decryption and decompression, the CRC will catch the error, but the authentication code will not.

    WinZip 9.0 and WinZip 10.0 used AE-2 for all files that they created, and did not store the CRC. As of WinZip 11, WinZip instead uses AE-1 for most files, storing the CRC as an additional integrity check against hardware or software errors occurring during the actual compression/encryption or decryption/decompression processes. WinZip 11 will continue to use AE-2, with no CRC, for very small files of less than 20 bytes. It will also use AE-2 for files compressed in BZIP2 format, because this format has internal integrity checks equivalent to a CRC check built in.

    Note that the AES-encrypted files created by WinZip 11 are fully compatible with January 2004's version 1.0.2 of the WinZip AES encryption specification, in which both the AE-1 and AE-2 variants of the file format were already defined.


**VII. Change history**

**Changes made in document version 1.04, January, 2009:**  Minor clarification regarding the algorithm used to generate the authentication code.

**Changes made in document version 1.03, November, 2006:**  Minor editorial and clarifying changes have been made throughout the document. The following substantive technical changes have been made:

1.  WinZip 11 Usage of AE-1 and AE-2

    WinZip's AES encryption specification defines two formats, known as AE-1 and AE-2, which differ in whether the CRC of the encrypted file is stored in the Zip headers. While the file formats themselves remain unchanged, WinZip's usage of them is changing. Beginning with  [WinZip 11](https://www.winzip.com/aes_info.htm#winzip11), WinZip uses the AE-1 format, which includes the CRC of the encrypted file, for many encrypted files, in order to provide an additional integrity check against hardware or software errors occurring during the compression/encryption or decryption/decompression processes. Note that AES-encrypted files created by WinZip 11 are completely compatible with the previous version of the WinZip encryption specification, January 2004's version 1.0.2.

2.  The discussion of  [salt values](https://www.winzip.com/aes_info.htm#salt)  mentions a limitation that applies to the uniqueness of salt values when very large numbers of files are encrypted with 128-bit encryption.
3.  Older versions of this specification suggested that other vendors might want to use their own vendor IDs to create their own unique encryption formats. We no longer suggest that vendor-specific alternative encryption methods be created in this way.

**Changes made in document version 1.02, January, 2004:**  The introductory text at the start of the document has been rewritten, and minor editorial and clarifying changes have been made throughout the document. Two substantive technical changes have been made:

1.  AE-2 Specification

    Standard Zip files store the CRC of each file's unencrypted data. This value is used to help detect damage or other alterations to Zip files. However, storing the CRC value has a drawback in that, for a very small file, such as a file of four or fewer bytes, the CRC value can be used, independent of the encryption algorithm, to help determine the unencrypted contents of the file.

    Because of this, files encrypted with the new AE-2 method store a 0 in the CRC field of the Zip header, and use the  [authentication code](https://www.winzip.com/aes_info.htm#authentication-code)  instead of the CRC value to verify that encrypted data within the Zip file has not been corrupted.

    The only differences between the AE-1 and AE-2 methods are the storage in AE-2 of 0 instead of the CRC in the Zip file header,and the use in the AES extra data field of 0x0002 for AE-2 instead of 0x0001 for AE-1 as the vendor version.

    Zip utilities that support the AE-2 format are required to be able to read files that were created in the AE-1 format, and during decryption/extraction of files in AE-1 format should verify that the file's CRC matches the value stored in the CRC field.

2.  Key Generation and HMAC-SHA1

    The description of the  [key generation](https://www.winzip.com/aes_info.htm#key-generation)  mechanism has been updated to point out a limitation arising from its use of HMAC-SHA1 as the pseudo-random function: When used in connection with 192- or 256-bit AES encryption, the fact that HMAC-SHA1 produces a 160-bit result means that, regardless of the password that you specify, the search space for the encryption key is unlikely to reach the theoretical 192- or 256-bit maximum, and cannot be guaranteed to exceed 160 bits. This is discussed in section B.1.1 of the  [RFC2898 specification](https://www.ietf.org/rfc/rfc2898.txt).

Document version: 1.04
Last modified: January 30, 2009

Copyright© 2003-2018 Corel Corporation.
All Rights Reserved
