/*
 * Copyright (c) 2019 by Milos Tosic. All Rights Reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef RMEM_HOOK_H
#define RMEM_HOOK_H

#include "../inc/rmem.h"
#include "rmem_config.h"
#include "rmem_utils.h"
#include "rmem_mutex.h"

#include "../../../../../Common_3/OS/Interfaces/IFileSystem.h"

namespace rmem {

	/// Memory hook interface class
	class MemoryHook
	{
		public:
			enum { OpBufferSize = 96 + (RMEM_STACK_TRACE_MAX * sizeof(uintptr_t)) };
			enum { BufferSize = RMEM_BUFFER_SIZE };

		private:
			bool		m_ignoreAllocs;
			uint8_t*	m_excessBufferPtr;
			size_t		m_bufferBytesWritten;
			uint8_t*	m_bufferPtr;
			uint8_t		m_bufferData[BufferSize * 2];
			uint8_t		m_excessBuffer[BufferSize];
#if RMEM_ENABLE_LZ4_COMPRESSION
			uint8_t		m_bufferCompressed[BufferSize];
#endif // RMEM_ENABLE_LZ4_COMPRESSION
			Mutex		m_mutexInternalBufferPtrs;
			Mutex		m_mutexWriteToFile;
			char		m_fileName[256];

			FileStream  m_file;
			size_t		m_excessBufferSize;
			int64_t		m_startTime;
			bool        m_fileValid;

			enum Enum
			{
				HashArraySize	= RMEM_STACK_TRACE_HASH_TABLE_SIZE,
				HashArrayMask	= HashArraySize - 1
			};
			uint32_t	m_stackTraceHashes[MemoryHook::HashArraySize];

			uintptr_t	m_stackTraces[MemoryHook::HashArraySize][RMEM_STACK_TRACE_MAX];

		public:
			MemoryHook(void* _data);
			~MemoryHook();

			/// Called on shut down to flush any queued data
			void flush();

			/// Called for each memory tag instantiation
			void registerTag(const char* _name, const char* _parentName);

			/// Called for each start of memory tag scope
			void enterTag(RMemTag& _tag);

			/// Called for each end of memory tag scope
			void leaveTag( RMemTag& _tag);

			/// Called for each memory marker instantiation
			void registerMarker(RMemMarker& _marker);

			/// Called for each memory marker occurance
			void marker(RMemMarker& _marker);

			/// Called for each heap registration
			void registerAllocator(const char* _name, uint64_t _handle);

			/// Called for each allocation
			void alloc(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead);

			/// Called for each reallocation
			void realloc(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead, void* _prevPtr);

			/// Called for each aligned allocation
			void allocAligned(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead, uint32_t _alignment);

			/// Called for each aligned reallocation
			void reallocAligned(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead, void* _prevPtr, uint32_t _alignment);

			/// Called for each free
			void free(uint64_t _handle, void* _ptr);

			/// Called for each loaded module
			void registerModule(const char* _name, uint64_t _base, uint32_t _size);

			/// Called for each loaded module
			void registerModule(const wchar_t* _name, uint64_t _base, uint32_t _size);

		private:
			/// Writes out a full stack trace
			void addStackTrace_new(uint8_t* _tmpBuffer, size_t& _tmpBuffPtr, uintptr_t* _stackTrace, uint32_t _numFrames);

			/// Called on each memory operation
			void addStackTrace(uint8_t* _tmpBuffer, size_t& _tmpBuffPtr, uintptr_t* _stackTrace, uint32_t _numTraces, uint32_t _stackHash);

			/// Writes data to the internal buffer
			void writeToBuffer(void* _ptr, size_t _size, uintptr_t* _stackTrace = 0, uint32_t _numFrames = 0);
		
			/// Writes data to file, used internally by writeToBuffer
			void writeToFile(void* _ptr, size_t _bytesToWrite);

			/// Dump additional debug info to help resolving symbols
			void writeModuleInfo();

			/// swap buffers
			uint8_t* doubleBuffer();
	};

} // namespace rmem

#endif // RMEM_HOOK_H
