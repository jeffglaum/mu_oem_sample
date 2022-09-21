/****************************************************************************\

    WIMFILE.H / Windows Imaging Project (OSIMAGE)

    Microsoft Confidential
    Copyright (c) Microsoft Corporation 2003
    All rights reserved

    Header file with the main structures for the image file
    format.

    03/2002 - Jason Cohen (JCOHEN)
    03/2002 - Bruce Green (BruceGr)

        Added this new header file for Windows Imaging (OSIMAGE) project.

\****************************************************************************/

#ifndef _WIMFILE_H_
#define _WIMFILE_H_

//
// 4200: Disable warning caused by structures having members like: MemberName[0].
// 4201: Disable warning caused by nameless struct/union.
//
#ifndef UEFI_BUILD_SYSTEM
#pragma warning(push)
#pragma warning(disable : 4200)
#pragma warning(disable : 4201)
#endif

// Need to define this guy here because this is where the hash
// structure is.
//
#define HASH_SIZE       A_SHA_DIGEST_LEN

//
// There are only two types of resources in the offset table,
// allocated (data) and unallocated (empty).
//

// This is the base resource header structure, which
// is the same as the free resource header and is also
// contained by the data resource.
//
typedef struct _RESHDR_BASE
{
    struct _RESHDR_BASE *lpNext;  // Must be first.
    union 
    {
        ULONGLONG ullSize;

        struct 
        {
            BYTE  sizebytes[7];
            BYTE  bFlags;
        };
    };
    LARGE_INTEGER liOffset;
}
RESHDR_BASE, *LPRESHDR_BASE,
RESHDR_EMPTY, *LPRESHDR_EMPTY;

//
// Since we manually pack ullSize and bFlags together, always use SIZEMASK to
// mask out bFlags from ullSize.  For both RESHDR_BASE & RESHDR_BASE_DISK.
//
#define SIZEMASK(ull) (ull & 0x00FFFFFFFFFFFFFF)


//
// For 32/64 bit independence, we use a packed on-disk structure without pointers...
//
#pragma pack(push,1)
typedef struct _RESHDR_BASE_DISK
{
    union 
    {
        ULONGLONG ullSize;

        struct 
        {
            BYTE  sizebytes[7];
            BYTE  bFlags;
        };
    };
    LARGE_INTEGER liOffset;
}
RESHDR_BASE_DISK, *LPRESHDR_BASE_DISK,
RESHDR_EMPTY_DISK, *LPRESHDR_EMPTY_DISK;

typedef struct _RESHDR_HASH_DATA_DISK
{
    union 
    {
        ULONGLONG ullSize;
        struct
        {
            DWORD dwSize;
            DWORD dwEncodingType;
        };
    };
    LARGE_INTEGER liOffset;
}
RESHDR_HASH_DATA_DISK, *LPRESHDR_HASH_DATA_DISK;

#pragma pack(pop)

// This field is valid if bFlags contains RESHDR_FLAG_CHUNKED
//
typedef struct _WIM_CHUNKED_INFO
{
    DWORD dwChunkNumber;
    DWORD dwFlags;
} WIM_CHUNKED_INFO, *LPWIM_CHUNKED_INFO;

#define WIM_CHUNK_FLAG_CHUNKED_REGION  (1)
#define WIM_CHUNK_FLAG_SPANNED         (2)

// This is the data resource header structure, which
// contains the base resource header.
//
typedef struct _RESHDR_DATA
{
    RESHDR_BASE             Base;  // Must be first.
    union
    {
        LARGE_INTEGER           liOriginalSize;
        WIM_CHUNKED_INFO        ChunkedInformation;
    };
    USHORT                  usPartNumber;
    DWORD                   dwRefCount;
    BYTE                    bHash[HASH_SIZE];
}
RESHDR_DATA, *LPRESHDR_DATA;

//
// For 32/64 bit independence, we use a packed on-disk structure without pointers...
//
#pragma pack(push,1)
typedef struct _RESHDR_DISK_SHORT
{
    RESHDR_BASE_DISK        Base;  // Must be first.
    LARGE_INTEGER           liOriginalSize;
}
RESHDR_DISK_SHORT, *LPRESHDR_DISK_SHORT;

#ifdef __cplusplus
typedef struct _RESHDR_DISK : public RESHDR_DISK_SHORT
{
#else
typedef struct _RESHDR_DISK
{
    RESHDR_BASE_DISK        Base;  // Must be first.
    LARGE_INTEGER           liOriginalSize;
#endif
    USHORT                  usPartNumber;
    DWORD                   dwRefCount;
    BYTE                    bHash[HASH_SIZE];
}
RESHDR_DISK, *LPRESHDR_DISK;

#pragma pack(pop)

// Here are the flags that are valid for the
// resources.
//
#define RESHDR_FLAG_FREE            0x01
#define RESHDR_FLAG_METADATA        0x02
#define RESHDR_FLAG_COMPRESSED      0x04
#define RESHDR_FLAG_SPANNED         0x08
#define RESHDR_FLAG_CHUNKED         0x10
#define RESHDR_FLAG_BACKED_BY_WIM   0x20
#define RESHDR_FLAG_UNUSED7         0x40
#define RESHDR_FLAG_UNUSED8         0x80

#define RESHDR_FLAG_VALID           (~(RESHDR_FLAG_UNUSED7 | RESHDR_FLAG_UNUSED8))

//
// Local Type Definition(s):
//

#define IMAGE_TAG   "MSWIM\0\0"

// This is the structure for the image
// header always found at the beginning
// of the image file.
//
// Note: Don't change anything in this without changing the packed version below...
//
typedef struct _WIMHEADER_V1
{
    CHAR            ImageTag[8];        // "MSWIM\0\0"
    DWORD           cbSize;
    DWORD           dwVersion;
    DWORD           dwFlags;
    DWORD           dwCompressionSize;
    GUID            gWIMGuid;
    USHORT          usPartNumber;
    USHORT          usTotalParts;
    DWORD           dwImageCount;
    RESHDR_DATA     rhOffsetTable;
    RESHDR_DATA     rhXmlData;
    RESHDR_DATA     rhBootMetadata;
    DWORD           dwBootIndex;
    RESHDR_DATA     rhIntegrity;
    BYTE            bWfsBlob[32];
    RESHDR_HASH_DATA_DISK rhCryptHashData; // The important part is its size is 16 bytes.
    BYTE            bUnused[12];
}
WIMHEADER_V1, *LPWIMHEADER_V1;

//
// For 32/64 bit independence, we use a packed on-disk structure without pointers...
//
#pragma pack(push,1)
typedef struct _WIMHEADER_V1_PACKED
{
    CHAR              ImageTag[8];        // "MSWIM\0\0"
    DWORD             cbSize;
    DWORD             dwVersion;
    DWORD             dwFlags;
    DWORD             dwCompressionSize;
    GUID              gWIMGuid;
    USHORT            usPartNumber;
    USHORT            usTotalParts;
    DWORD             dwImageCount;
    RESHDR_DISK_SHORT rhOffsetTable;
    RESHDR_DISK_SHORT rhXmlData;
    RESHDR_DISK_SHORT rhBootMetadata;
    DWORD             dwBootIndex;
    RESHDR_DISK_SHORT rhIntegrity;
    BYTE              bWfsBlob[32];
    RESHDR_HASH_DATA_DISK rhCryptHashData;
    BYTE              bUnused[12];
}
WIMHEADER_V1_PACKED, *LPWIMHEADER_V1_PACKED;
#pragma pack(pop)

//
// Flags for the dwFlags field...
//
#define FLAG_HEADER_RESERVED            0x00000001
#define FLAG_HEADER_COMPRESSION         0x00000002
#define FLAG_HEADER_READONLY            0x00000004
#define FLAG_HEADER_SPANNED             0x00000008
#define FLAG_HEADER_RESOURCE_ONLY       0x00000010
#define FLAG_HEADER_METADATA_ONLY       0x00000020
#define FLAG_HEADER_WRITE_IN_PROGRESS   0x00000040
#define FLAG_HEADER_RP_FIX              0x00000080  // reparse point fixup

//
// Compression types are in upper word of flags...
//
#define FLAG_HEADER_COMPRESS_RESERVED   0x00010000
#define FLAG_HEADER_COMPRESS_XPRESS     0x00020000
#define FLAG_HEADER_COMPRESS_LZX        0x00040000
#define FLAG_HEADER_COMPRESS_LZMS       0x00080000
#define FLAG_HEADER_COMPRESS_NEW_XPRESS 0x00100000
#define FLAG_HEADER_COMPRESS_NEW_XPRESS_HUFF 0x00200000
#define FLAG_HEADER_COMPRESS_LZNT1      0xFF000000

//
// Use the following typedef to assign IMGHEADER as the latest IMGHEADER_Vx structure
//
typedef WIMHEADER_V1        WIMHEADER, *LPWIMHEADER;
typedef WIMHEADER_V1_PACKED WIMHEADER_PACKED, *LPWIMHEADER_PACKED;

//
// Integrity structure
//
#pragma pack(push,1)
typedef struct _WIMHASH
{
    DWORD cbSize;
    DWORD dwNumElements;
    DWORD dwChunkSize;
    BYTE  abHashList[0];
}
WIMHASH, *LPWIMHASH;
#pragma pack(pop)

//
// Chunked region header
//
#pragma pack(push,1)
typedef struct _WIM_CHUNKED_REGION_HEADER
{
    ULONGLONG ullUncompressedSize;
    DWORD dwWindowSize;
    DWORD dwCompressionType;
    DWORD dwCompressedSizes[0];
} WIM_CHUNKED_REGION_HEADER, *LPWIM_CHUNKED_REGION_HEADER;
#pragma pack(pop)

//
// Alignment macros
//
#define WordAlignPtr(p) ( (PVOID)((((ULONG_PTR)(p)) + 1) & (-2)) )
#define LongAlignPtr(p) ( (PVOID)((((ULONG_PTR)(p)) + 3) & (-4)) )
#define QuadAlignPtr(p) ( (PVOID)((((ULONG_PTR)(p)) + 7) & (-8)) )

#define WordAlign(p) ( ((((p)) + 1) & (-2)) )
#define LongAlign(p) ( ((((p)) + 3) & (-4)) )
#define QuadAlign(p) ( ((((p)) + 7) & (-8)) )

#ifndef UEFI_BUILD_SYSTEM
#pragma warning(pop)
#endif

#endif // _WIMFILE_H_
