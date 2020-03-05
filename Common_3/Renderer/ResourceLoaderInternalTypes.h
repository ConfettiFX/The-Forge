#ifndef ResourceLoaderInternalTypes_h
#define ResourceLoaderInternalTypes_h

#ifdef __cplusplus
extern "C" {
#endif

// The ResourceLoader UpdateDesc and LoadDesc types store internal state at the end of the struct.
// The types for this internal state need to be defined, but we don't want them cluttering up the ResourceLoader interface,
// so instead we define them here.

typedef struct MappedMemoryRange
{
	uint8_t* pData;
	Buffer*  pBuffer;
	uint64_t mOffset;
	uint64_t mSize;
} MappedMemoryRange;

typedef struct UMAAllocation {
    void* pData;
    void* pAllocationInfo;
} UMAAllocation;

typedef struct BufferUpdateInternalData {
	MappedMemoryRange mMappedRange;
	bool mBufferNeedsUnmap;
} BufferUpdateInternalData;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* ResourceLoaderInternalTypes_h */
