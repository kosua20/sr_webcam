#include "sr_webcam_internal.h"


#ifdef __cplusplus

#include <windows.h>
#include <guiddef.h>
#include <mfidl.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfobjects.h>
#include <tchar.h>
#include <strsafe.h>
#include <mfreadwrite.h>
#include <new>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <mferror.h>
#include <comdef.h>
#include <shlwapi.h>  // QISearch

struct IMFMediaType;
struct IMFActivate;
struct IMFMediaSource;
struct IMFAttributes;

namespace {
	// Ptr
	template <class T>
	class ComPtr {
	public:
		ComPtr(){}
		
		ComPtr(T* lp){
			p = lp;
		}
		
		ComPtr(_In_ const ComPtr<T>& lp){
			p = lp.p;
		}
		
		virtual ~ComPtr(){}
		
		T** operator&(){
			return p.operator&();
		}
		
		T* operator->() const {
			return p.operator->();
		}
		
		operator bool(){
			return p.operator!=(NULL);
		}
		
		T* Get() const {
			return p;
		}
		
		void Release(){
			if (p){
				p.Release();
			}
		}
		
		template<typename U>
		HRESULT As(_Out_ ComPtr<U>& lp) const{
			lp.Release();
			return p->QueryInterface(__uuidof(U), reinterpret_cast<void**>((T**)&lp));
		}
		
	private:
		_COM_SMARTPTR_TYPEDEF(T, __uuidof(T));
		TPtr p;
	};
	
	#define _ComPtr ComPtr
	
	// Context
	class MFContext {
	public:
		static MFContext& getContext(){
			static MFContext instance;
			return instance;
		}
		
		~MFContext(void) {
			CoUninitialize();
		}
		
	private:
		MFContext(void) {
			CoInitialize(0);
			SUCCEEDED(MFStartup(MF_VERSION));
		}
	};


}

struct Format {
	
	unsigned int frameSize = 0;
	UINT32 height = 0;
	UINT32 width = 0;
	int stride = 0; // stride is negative if image is bottom-up
	unsigned int fixedSizeSamples = 0;
	UINT32 frameRateNum = 0;
	UINT32 frameRateDenom = 0;
	unsigned int sampleSize = 0;
	unsigned int interlaceMode = 0;
	GUID type;
	GUID subType;
	double framerate = 0.0;
	LONGLONG frameStep = 0;
	
	Format(){
		memset(&type, 0, sizeof(GUID));
		memset(&subType, 0, sizeof(GUID));
	}
	
	Format(IMFMediaType *pType){
		memset(&type, 0, sizeof(GUID));
		memset(&subType, 0, sizeof(GUID));
		
		UINT32 count = 0;
		if(SUCCEEDED(pType->GetCount(&count)) && SUCCEEDED(pType->LockStore())) {
			for (UINT32 i = 0; i < count; i++){
				PROPVARIANT var;
				PropVariantInit(&var);
				GUID guid = { 0 };
				if(!SUCCEEDED(pType->GetItemByIndex(i, &guid, &var))){
					continue;
				}
				// Extract the properties we need.
				if(guid == MF_MT_DEFAULT_STRIDE && var.vt == VT_INT){
					stride = var.intVal;
				} else if(guid == MF_MT_FRAME_RATE && var.vt == VT_UI8){
					Unpack2UINT32AsUINT64(var.uhVal.QuadPart, &frameRateNum, &frameRateDenom);
					// Compute framerate.
					if(frameRateDenom != 0){
						framerate = ((double)frameRateNum) / ((double)frameRateDenom);
					}
					frameStep = (LONGLONG)(framerate > 0 ? (1e7 / framerate) : 0);
				} else if(guid == MF_MT_DEFAULT_STRIDE && var.vt == VT_UI4){
					stride = (int)var.ulVal;
				} else if(guid == MF_MT_FIXED_SIZE_SAMPLES && var.vt == VT_UI4){
					fixedSizeSamples = var.ulVal;
				} else if(guid == MF_MT_SAMPLE_SIZE && var.vt == VT_UI4){
					sampleSize = var.ulVal;
				} else if(guid == MF_MT_INTERLACE_MODE && var.vt == VT_UI4){
					interlaceMode = var.ulVal;
				} else if(guid == MF_MT_MAJOR_TYPE && var.vt == VT_CLSID){
					type = *var.puuid;
				} else if(guid == MF_MT_SUBTYPE && var.vt == VT_CLSID){
					subType = *var.puuid;
				} else if(guid == MF_MT_FRAME_SIZE && var.vt == VT_UI8) {
					Unpack2UINT32AsUINT64(var.uhVal.QuadPart, &width, &height);
					// Compute frame size.
					frameSize = width * height;
				}
				PropVariantClear(&var);
			}
			pType->UnlockStore();
		}
	}
};

class VideoStreamMediaFoundation : public IMFSourceReaderCallback {
public:
	
	VideoStreamMediaFoundation() : context(MFContext::getContext()) {
		
	}
	
	virtual ~VideoStreamMediaFoundation(){
		
	}

	STDMETHODIMP QueryInterface(  REFIID riid,  _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4838)
#endif
		static const QITAB qit[] = {
			QITABENT(VideoStreamMediaFoundation, IMFSourceReaderCallback), { 0 }, };
#ifdef _MSC_VER
#pragma warning(pop)
#endif
		return QISearch(this, qit, riid, ppvObject);
	};

	STDMETHODIMP_(ULONG) AddRef() override {
		return InterlockedIncrement(&refCount);
	}
	STDMETHODIMP_(ULONG) Release() override {
		ULONG uCount = InterlockedDecrement(&refCount);
		if (uCount == 0) {
			delete this;
		}
		return uCount;
	}

	STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample) override {
		printf("Bup\n");
		sourceReader->ReadSample(dwStreamIndex, 0, NULL, NULL, NULL, NULL);
		return S_OK;	
	}

	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *) override {
		return S_OK;
	}

	STDMETHODIMP OnFlush(DWORD) override {
		return S_OK;
	}

	bool setupWith(int id, int framerate, int w, int h){
		ComPtr<IMFAttributes> msAttr = NULL;
		if(!(SUCCEEDED(MFCreateAttributes(&msAttr, 1)) &&
			 SUCCEEDED(msAttr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID )))){
			return false;
		}
		IMFActivate **ppDevices = NULL;
		UINT32 count = 0;
		bool opened = false;
		// Enumerate devices.
		if(!SUCCEEDED(MFEnumDeviceSources(msAttr.Get(), &ppDevices, &count)) || count == 0 || id < 0){
			CoTaskMemFree(ppDevices);
			return false;
		}
		_id = min((int)count-1, id);
		// Release all the others.
		for(int i = 0; i < (int)count; ++i){
			if(i == _id || !ppDevices[i]){
				continue;
			}
			ppDevices[i]->Release();
		}
		// If the device is null, not available.
		if(!ppDevices[_id]){
			CoTaskMemFree(ppDevices);
			return false;
		}
		
		// Set source reader parameters
		_ComPtr<IMFMediaSource> mSrc;
		_ComPtr<IMFAttributes> srAttr;
		
		if(!SUCCEEDED(ppDevices[_id]->ActivateObject( __uuidof(IMFMediaSource), (void**)&mSrc )) || !mSrc){
			ppDevices[_id]->Release();
			CoTaskMemFree(ppDevices);
			return false;
		}
		
		if(!SUCCEEDED(MFCreateAttributes(&srAttr, 10))){
			ppDevices[_id]->Release();
			CoTaskMemFree(ppDevices);
			return false;
		}
		
		srAttr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		srAttr->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, FALSE);
		srAttr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, FALSE);
		srAttr->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
		
		// Define callback.
		HRESULT res = srAttr->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, (IMFSourceReaderCallback*)this);
		if(FAILED(res)){
			ppDevices[_id]->Release();
			CoTaskMemFree(ppDevices);
			return false;
		}
		
		if(!SUCCEEDED(MFCreateSourceReaderFromMediaSource(mSrc.Get(), srAttr.Get(), &videoFileSource))){
			ppDevices[_id]->Release();
			CoTaskMemFree(ppDevices);
			return false;
		}
		
		// Look at the valid formats.
		HRESULT hr = S_OK;
		
		int bestStream = -1;
		Format bestFormat;
		float bestFit = 1e9;
		
		DWORD streamId = 0;
		DWORD typeId = 0;
		
		// Iterate over streams and media types.
		while(SUCCEEDED(hr)) {
			_ComPtr<IMFMediaType> pType;
			hr = videoFileSource->GetNativeMediaType(streamId, typeId, &pType);
			// If we reach the end of format types for this stream, move to the next.
			if(hr == MF_E_NO_MORE_TYPES) {
				hr = S_OK;
				++streamId;
				typeId = 0;
				continue;
			}
			// If hr is neither no_more_types nor success, we reached the end of the streams list.
			if(!SUCCEEDED(hr)) {
				continue;
			}
			Format format(pType.Get());
			// We only care about video types.
			if(format.type != MFMediaType_Video) {
				++typeId;
				continue;
			}
			// Init with the first existing format.
			if(bestStream < 0){
				const float dw = (float)(w - format.width);
				const float dh = (float)(h - format.height);
				bestFit = sqrtf(dw*dw+dh*dh);
				bestStream = (int)streamId;
				bestFormat = format;
				++typeId;
				continue;
			}
			// If the current best already has the same size, replace it if the framerate is closer to the requested one.
			if(format.width == bestFormat.width && format.height == bestFormat.height){
				if(abs(framerate - format.framerate) < abs(framerate - bestFormat.framerate)){
					bestStream = (int)streamId;
					bestFormat = format;
					
				}
				++typeId;
				continue;
			}
			// Else, replace the format if its size is closer to the required one.
			const float dw = (float)(w - format.width);
			const float dh = (float)(h - format.height);
			const float fit = sqrtf(dw*dw+dh*dh);
			if(fit < bestFit){
				bestFit = fit;
				bestStream = (int)streamId;
				bestFormat = format;
			}
			++typeId;
		}
		
		if(bestStream < 0){
			ppDevices[_id]->Release();
			CoTaskMemFree(ppDevices);
			return false;
		}
		

		// We found the best available stream and format, configure.
		GUID outSubtype = MFVideoFormat_RGB24; // Note from OpenCV: HW only supports MFVideoFormat_RGB32.
		UINT32 outStride = 3 * bestFormat.width;
		UINT32 outSize = outStride * bestFormat.height;
		
		_ComPtr<IMFMediaType> typeOut;
		if(!SUCCEEDED(MFCreateMediaType(&typeOut))){
			ppDevices[_id]->Release();
			CoTaskMemFree(ppDevices);
			return false;
		}
		typeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		typeOut->SetGUID(MF_MT_SUBTYPE, outSubtype);
		typeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		MFSetAttributeRatio(typeOut.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		MFSetAttributeSize(typeOut.Get(), MF_MT_FRAME_SIZE, bestFormat.width, bestFormat.height);
		// Should we specify the output framerate or is this controlled by the native input format?
		MFSetAttributeRatio(typeOut.Get(), MF_MT_FRAME_RATE, bestFormat.frameRateNum, bestFormat.frameRateDenom);
		typeOut->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, 1);
		typeOut->SetUINT32(MF_MT_SAMPLE_SIZE, outSize);
		typeOut->SetUINT32(MF_MT_DEFAULT_STRIDE, outStride);
		
		if(!SUCCEEDED(videoFileSource->SetStreamSelection ((DWORD)MF_SOURCE_READER_ALL_STREAMS, false)) ||
		   !SUCCEEDED(videoFileSource->SetStreamSelection ((DWORD)bestStream, true)) ||
		   !SUCCEEDED(videoFileSource->SetCurrentMediaType((DWORD)bestStream, NULL, typeOut.Get()))){
			videoFileSource.Release();
			ppDevices[_id]->Release();
			CoTaskMemFree(ppDevices);
			return false;
		}
		
		selectedStream = (DWORD)bestStream;
		nativeFormat = bestFormat;
		captureFormat = Format(typeOut.Get());
		ppDevices[_id]->Release();
		CoTaskMemFree(ppDevices);
		
		return true;
	}
	
	void start(){
		// Initiate capturing with async callback
		sourceReader = videoFileSource.Get();
		if (FAILED(videoFileSource->ReadSample(selectedStream, 0, NULL, NULL, NULL, NULL))){
			printf("Failed.\n");
			sourceReader = NULL;
		}
	}
	
	void stop(){
		videoFileSource.Release();
		
	}

public:
	
	sr_webcam_device * _parent = NULL;
	int _id = -1;
	Format captureFormat;
	
private:
	MFContext & context;
	//_ComPtr<IMFSourceReaderCallback> readCallback;
	_ComPtr<IMFSourceReader> videoFileSource;
	DWORD selectedStream;
	Format nativeFormat;
	long refCount = 0;
	IMFSourceReader * sourceReader;
	_ComPtr<IMFSample> lastSample;
};

int sr_webcam_open(sr_webcam_device * device) {
	VideoStreamMediaFoundation * stream = new VideoStreamMediaFoundation();
	stream->_parent = device;
	bool res = stream->setupWith(device->deviceId, device->framerate, device->width, device->height);
	if(!res){
		return -1;
	}
	device->stream = stream;
	device->width = stream->captureFormat.width;
	device->height = stream->captureFormat.height;
	device->framerate = (int)(stream->captureFormat.framerate);
	device->deviceId = stream->_id;
	return 0;
}


void sr_webcam_start(sr_webcam_device * device) {
	if (device->stream) {
		VideoStreamMediaFoundation * stream = (VideoStreamMediaFoundation*)(device->stream);
		stream->start();
	}
}

void sr_webcam_stop(sr_webcam_device * device) {
	if (device->stream) {
		VideoStreamMediaFoundation * stream = (VideoStreamMediaFoundation*)(device->stream);
		//stream->stop();
	}
}

#endif
