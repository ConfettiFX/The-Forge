#pragma once

#include <Windows.h>
#include <thumbcache.h>

/**
 * 
 */
class BasisThumbProvider : public IInitializeWithStream, public IThumbnailProvider
{
public:
	BasisThumbProvider();
	// IUnknown::QueryInterface()
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
	// IUnknown::AddRef()
	IFACEMETHODIMP_(ULONG) AddRef() override;
	// IUnknown::Release()
	IFACEMETHODIMP_(ULONG) Release() override;
	
	// IInitializeWithStream::Initialize()
	IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode) override;
	
	// IThumbnailProvider::GetThumbnail()
	IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) override;

protected:
	virtual ~BasisThumbProvider();
	
private:
	LONG count;
	IStream* stream;
};

/**
 * 
 */
class BasisThumbProviderFactory : public IClassFactory
{
public:
	BasisThumbProviderFactory();
	// IUnknown::QueryInterface()
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
	// IUnknown::AddRef()
	IFACEMETHODIMP_(ULONG) AddRef() override;
	// IUnknown::Release()
	IFACEMETHODIMP_(ULONG) Release() override;
	
	// IClassFactory::CreateInstance()
	IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) override;
	// IClassFactory::LockServer()
	IFACEMETHODIMP LockServer(BOOL fLock) override;
	
protected:
	virtual ~BasisThumbProviderFactory();
	
private:
	LONG count;
};
