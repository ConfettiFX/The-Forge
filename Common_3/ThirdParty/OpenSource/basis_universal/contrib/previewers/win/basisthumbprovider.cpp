#include "basisthumbprovider.h"

#include <Shlwapi.h>

#include "basisu_transcoder.h"

#include "helpers.h"

#pragma comment(lib, "Shlwapi.lib")

using namespace basist;

static etc1_global_selector_codebook* globalCodebook = NULL;

BasisThumbProvider::BasisThumbProvider() : count(1), stream(NULL) {
	dprintf("BasisThumbProvider ctor");
	basisu_transcoder_init();
	if (!globalCodebook) {
		 globalCodebook = new etc1_global_selector_codebook(g_global_selector_cb_size, g_global_selector_cb);
	}
}

BasisThumbProvider::~BasisThumbProvider() {
	dprintf("BasisThumbProvider **dtor**");
	if (stream) {
		stream->Release();
		stream = NULL;
	}
}

IFACEMETHODIMP BasisThumbProvider::QueryInterface(REFIID riid, void **ppv) {
	static const QITAB qit[] = {
		QITABENT(BasisThumbProvider, IThumbnailProvider),
		QITABENT(BasisThumbProvider, IInitializeWithStream),
		{0},
	};
	return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) BasisThumbProvider::AddRef() {
	return InterlockedIncrement(&count);
}

IFACEMETHODIMP_(ULONG) BasisThumbProvider::Release() {
	LONG refs = InterlockedDecrement(&count);
	if (refs == 0) {
		delete this;
	}
	return refs;
}

IFACEMETHODIMP BasisThumbProvider::Initialize(IStream *pStream, DWORD grfMode) {
	dprintf("BasisThumbProvider::Initialize");
	HRESULT hr = HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
	if (!stream) {
		hr = pStream->QueryInterface(&stream);
	}
	return hr;
}

// Note: thumbnails get written here: %LocalAppData%\Microsoft\Windows\Explorer
IFACEMETHODIMP BasisThumbProvider::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) {
	STATSTG stat;
	if (stream && SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME))) {
		if (void* data = malloc(static_cast<size_t>(stat.cbSize.LowPart))) {
			ULONG size = 0;
			if (SUCCEEDED(stream->Read(data, static_cast<ULONG>(stat.cbSize.LowPart), &size))) {
				if (size == stat.cbSize.LowPart) {
					basisu_transcoder transcoder(globalCodebook);
					if (transcoder.validate_header(data, size)) {
						dprintf("Requested %d bytes for %dx%d image", size, cx, cx);
						basisu_image_info info;
						if (transcoder.get_image_info(data, size, info, 0)) {
							uint32_t level = 0;
							uint32_t descW = 0, descH = 0, blocks;
							for (uint32_t n = 0; n < info.m_total_levels; n++) {
								if (transcoder.get_image_level_desc(data, size, 0, n, descW, descH, blocks)) {
									dprintf("mipmap level w: %d, h: %d (blocks: %d)", descW, descH, blocks);
									if (cx >= std::max(descW, descH)) {
										level = n;
										break;
									}
								}
							}
							basisu_file_info fileInfo;
							transcoder.get_file_info(data, size, fileInfo);
							if (transcoder.start_transcoding(data, size)) {
								if (void* rgbBuf = malloc(basis_get_bytes_per_block(transcoder_texture_format::cTFRGBA32) * blocks)) {
									// Note: the API expects total pixels here instead of blocks
									if (transcoder.transcode_image_level(data, size, 0, level, rgbBuf, descW * descH, transcoder_texture_format::cTFRGBA32)) {
										dprintf("Decoded!!!!");
										*phbmp = rgbToBitmap(static_cast<uint32_t*>(rgbBuf), descW, descH, fileInfo.m_y_flipped);
									}
									delete rgbBuf;
								}
							}
						}
					}
				}
			}
			free(data);
		}
	}
	return (*phbmp) ? S_OK : S_FALSE;
}

//********************************** Factory *********************************/

BasisThumbProviderFactory::BasisThumbProviderFactory() : count(1) {}

BasisThumbProviderFactory::~BasisThumbProviderFactory() {}

IFACEMETHODIMP BasisThumbProviderFactory::QueryInterface(REFIID riid, void **ppv) {
	static const QITAB qit[] = {
		QITABENT(BasisThumbProviderFactory, IClassFactory),
		{0},
	};
	return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) BasisThumbProviderFactory::AddRef() {
	return InterlockedIncrement(&count);
}

IFACEMETHODIMP_(ULONG) BasisThumbProviderFactory::Release() {
	LONG refs = InterlockedDecrement(&count);
	if (refs == 0) {
		delete this;
	}
	return refs;
}

IFACEMETHODIMP BasisThumbProviderFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) {
	HRESULT hr = CLASS_E_NOAGGREGATION;
	if (pUnkOuter == NULL) {
		hr = E_OUTOFMEMORY;
		if (BasisThumbProvider* provider = new (std::nothrow) BasisThumbProvider()) {
			hr = provider->QueryInterface(riid, ppv);
			provider->Release();
		}
	}
	return hr;
}

IFACEMETHODIMP BasisThumbProviderFactory::LockServer(BOOL fLock) {
	if (fLock) {
		InterlockedIncrement(&count);
	} else {
		InterlockedDecrement(&count);
	}
	return S_OK;
}
