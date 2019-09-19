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
#include <shlwapi.h>

struct IMFMediaType;
struct IMFActivate;
struct IMFMediaSource;
struct IMFAttributes;

namespace {
	
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

	STDMETHODIMP QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override {
		// Based on OpenCV.
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
		
		if(!_parent){
			return S_OK;
		}
		
		if(!SUCCEEDED(hrStatus)){
			return S_OK;
		}
		
		if(pSample != NULL){
			
			IMFMediaBuffer * buffer = NULL;
			if(!SUCCEEDED(pSample->ConvertToContiguousBuffer(&buffer))){
				// Try to get direct access to the buffer.
				DWORD bcnt = 0;
				if(!SUCCEEDED(pSample->GetBufferCount(&bcnt)) || bcnt == 0){
					return S_OK;
				}
				if(!SUCCEEDED(pSample->GetBufferByIndex(0, &buffer))){
					return S_OK;
				}
			}

			// As in OpenCV, we use a lock2D if possible.
			bool is2DLocked = false;
			BYTE* ptr = NULL;
			LONG pitch = 0;
			DWORD maxsize = 0, cursize = 0;
			IMF2DBuffer * buffer2d = NULL;
			
			// Try to convert the buffer.
			HRESULT res1 = buffer->QueryInterface(__uuidof(IMF2DBuffer), reinterpret_cast<void**>((IMFMediaBuffer**)&buffer2d));
			
			if(res1){
				if(SUCCEEDED(buffer2d->Lock2D(&ptr, &pitch))){
					is2DLocked = true;
				}
			}
			
			// Maybe the 2D lock failed, try a regular one.
			if(!is2DLocked){
				if(!SUCCEEDED(buffer->Lock(&ptr, &maxsize, &cursize))){
					return S_OK;
				}
			}
			if(!ptr){
				return S_OK;
			}
			if(!is2DLocked && ((unsigned int)cursize != captureFormat.sampleSize)){
				buffer->Unlock();
				return S_OK;
			}
			// Assume BGR for now.
			unsigned char *dstBuffer = (unsigned char *)malloc(captureFormat.width*captureFormat.height * 3);
			// Copy each row, taking the pitch into account. Maybe we should check the number of channels also?
			const unsigned int exactRowSize = captureFormat.width * 3;
			if(pitch == 0){
				pitch = exactRowSize;
			}
			for(unsigned int y = 0; y < captureFormat.height; ++y){
				for(unsigned int x = 0; x < captureFormat.width; ++x){
					dstBuffer[y*exactRowSize+3*x+0] = ptr[y*pitch+3*x+2];
					dstBuffer[y*exactRowSize+3*x+1] = ptr[y*pitch+3*x+1];
					dstBuffer[y*exactRowSize+3*x+2] = ptr[y*pitch+3*x+0];
				}
			}
			_parent->callback(_parent, dstBuffer);

			if(is2DLocked){
				buffer2d->Unlock2D();
			} else {
				buffer->Unlock();
			}
		}

		HRESULT res = videoReader->ReadSample(dwStreamIndex, 0, NULL, NULL, NULL, NULL);
		if(FAILED(res)){
			// scheduling failed, reached end of file, ...
			return S_OK;
		}
		return S_OK;	
	}

	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *) override {
		return S_OK;
	}

	STDMETHODIMP OnFlush(DWORD) override {
		return S_OK;
	}

	bool setupWith(int id, int framerate, int w, int h){
		IMFAttributes * msAttr = NULL;
		if(!(SUCCEEDED(MFCreateAttributes(&msAttr, 1)) &&
			 SUCCEEDED(msAttr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID )))){
			return false;
		}
		IMFActivate **ppDevices = NULL;
		UINT32 count = 0;
		bool opened = false;
		// Enumerate devices.
		if(!SUCCEEDED(MFEnumDeviceSources(msAttr, &ppDevices, &count)) || count == 0 || id < 0){
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
		IMFMediaSource * mSrc = NULL;
		IMFAttributes * srAttr = NULL;
		
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
		
		if(!SUCCEEDED(MFCreateSourceReaderFromMediaSource(mSrc, srAttr, &videoReader))){
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
			IMFMediaType * pType = NULL;
			hr = videoReader->GetNativeMediaType(streamId, typeId, &pType);
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
			Format format(pType);
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
		GUID outSubtype = MFVideoFormat_RGB24;
		UINT32 outStride = 3 * bestFormat.width;
		UINT32 outSize = outStride * bestFormat.height;
		
		IMFMediaType * typeOut = NULL;
		if(!SUCCEEDED(MFCreateMediaType(&typeOut))){
			ppDevices[_id]->Release();
			CoTaskMemFree(ppDevices);
			return false;
		}
		typeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		typeOut->SetGUID(MF_MT_SUBTYPE, outSubtype);
		typeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		MFSetAttributeRatio(typeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		MFSetAttributeSize(typeOut, MF_MT_FRAME_SIZE, bestFormat.width, bestFormat.height);
		// Should we specify the output framerate or is this controlled by the native input format?
		MFSetAttributeRatio(typeOut, MF_MT_FRAME_RATE, min(framerate, (int)bestFormat.framerate),1);
		typeOut->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, 1);
		typeOut->SetUINT32(MF_MT_SAMPLE_SIZE, outSize);
		typeOut->SetUINT32(MF_MT_DEFAULT_STRIDE, outStride);
		
		if(!SUCCEEDED(videoReader->SetStreamSelection ((DWORD)MF_SOURCE_READER_ALL_STREAMS, false)) ||
		   !SUCCEEDED(videoReader->SetStreamSelection ((DWORD)bestStream, true)) ||
		   !SUCCEEDED(videoReader->SetCurrentMediaType((DWORD)bestStream, NULL, typeOut))){
			delete videoReader;
			videoReader = NULL;
			ppDevices[_id]->Release();
			CoTaskMemFree(ppDevices);
			return false;
		}
		
		selectedStream = (DWORD)bestStream;
		captureFormat = Format(typeOut);
		ppDevices[_id]->Release();
		CoTaskMemFree(ppDevices);
		
		return true;
	}
	
	void start(){
		if (FAILED(videoReader->ReadSample(selectedStream, 0, NULL, NULL, NULL, NULL))){
			printf("Failed.\n");
			delete videoReader;
			videoReader = NULL;
		}
	}
	
	void stop(){
		delete videoReader;
		videoReader = NULL;
	}

public:
	
	sr_webcam_device * _parent = NULL;
	int _id = -1;
	Format captureFormat;
	
private:
	MFContext & context;
	IMFSourceReader * videoReader;
	DWORD selectedStream;
	long refCount = 0;

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
