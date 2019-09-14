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
#ifdef _MSC_VER
#pragma warning(disable:4503)
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "Strmiids")
#pragma comment(lib, "Mfreadwrite")
#pragma comment(lib, "Shlwapi.lib")
#endif
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


int sr_webcam_setup(sr_webcam_device * device) {
	MFContext & contex = MFContext::getContext();
	_ComPtr<IMFSourceReaderCallback> readCallback;
	_ComPtr<IMFSourceReader> videoFileSource;
	LONGLONG frameStep;
	
	
	if(device->deviceId < 0){
		return 1;
	}
	
	ComPtr<IMFAttributes> msAttr = NULL;
	if(!(SUCCEEDED(MFCreateAttributes(&msAttr, 1)) &&
		SUCCEEDED(msAttr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID )))){
		return 1;
	}
	IMFActivate **ppDevices = NULL;
	UINT32 count;
	bool opened = false;
	// Enumerate devices.
	if(SUCCEEDED(MFEnumDeviceSources(msAttr.Get(), &ppDevices, &count))){
		if(count > 0){
			for(int i = 0; i < (int)count; ++i){
				if(!ppDevices[i]){
					continue;
				}
				if(i == device->deviceId){
					// Set source reader parameters
					_ComPtr<IMFMediaSource> mSrc;
					_ComPtr<IMFAttributes> srAttr;
					
					if(!SUCCEEDED( ppDevices[ind]->ActivateObject( __uuidof(IMFMediaSource), (void**)&mSrc ))){
						continue;
					}
					if(!mSrc){
						continue;
					}
					if(!SUCCEEDED(MFCreateAttributes(&srAttr, 10))){
						continue;
					}
					if(!SUCCEEDED(srAttr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE))){
						continue;
					}
					if(!SUCCEEDED(srAttr->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, FALSE))){
						continue;
					}
					if(!SUCCEEDED(srAttr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, FALSE))){
						continue;
					}
					if(!SUCCEEDED(srAttr->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE))){
						continue;
					}
					// Define callback.
					//readCallback = ComPtr<IMFSourceReaderCallback>(new SourceReaderCB());
					HRESULT hr = srAttr->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, (IMFSourceReaderCallback*)readCallback.Get());
					if(FAILED(hr)){
						readCallback.Release();
						continue;
					}
					
					if(SUCCEEDED(MFCreateSourceReaderFromMediaSource(mSrc.Get(), srAttr.Get(), &videoFileSource))){
						opened = true;
						// Look at the valid formats.
						HRESULT hr = S_OK;
						int dwStreamBest = -1;
						//MediaType MTBest;
						
						DWORD dwMediaTypeTest = 0;
						DWORD dwStreamTest = 0;
						while(SUCCEEDED(hr)) {
							_ComPtr<IMFMediaType> pType;
							hr = videoFileSource->GetNativeMediaType(dwStreamTest, dwMediaTypeTest, &pType);
							if(hr == MF_E_NO_MORE_TYPES) {
								hr = S_OK;
								++dwStreamTest;
								dwMediaTypeTest = 0;
								continue;
							} else if (SUCCEEDED(hr)) {
								MediaType MT(pType.Get());
								if (MT.MF_MT_MAJOR_TYPE == MFMediaType_Video)
								{
									if (dwStreamBest < 0 ||
										resolutionDiff(MT, width, height) < resolutionDiff(MTBest, width, height) ||
										(resolutionDiff(MT, width, height) == resolutionDiff(MTBest, width, height) && MT.width > MTBest.width) ||
										(resolutionDiff(MT, width, height) == resolutionDiff(MTBest, width, height) && MT.width == MTBest.width && MT.height > MTBest.height) ||
										(MT.width == MTBest.width && MT.height == MTBest.height && (getFramerate(MT) > getFramerate(MTBest) && (prefFramerate == 0 || getFramerate(MT) <= prefFramerate)))
										)
									{
										dwStreamBest = (int)dwStreamTest;
										MTBest = MT;
									}
								}
								++dwMediaTypeTest;
							}
						}
						// Configure.
						if (dwStreamBest >= 0)
						{
							GUID outSubtype = GUID_NULL;
							UINT32 outStride = 0;
							UINT32 outSize = 0;
							// We do conversion if needed.
							outSubtype =MFVideoFormat_RGB24; // HW accelerated mode support only RGB32 captureMode == MODE_HW ? MFVideoFormat_RGB32 :
							outStride = 3 * MTBest.width; //(captureMode == MODE_HW ? 4 : 3)
							outSize = outStride * MTBest.height;
							
							_ComPtr<IMFMediaType>  mediaTypeOut;
							if (// Set the output media type.
								SUCCEEDED(MFCreateMediaType(&mediaTypeOut)) &&
								SUCCEEDED(mediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
								SUCCEEDED(mediaTypeOut->SetGUID(MF_MT_SUBTYPE, convertToFormat ? outSubtype : MTBest.MF_MT_SUBTYPE)) &&
								SUCCEEDED(mediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, convertToFormat ? MFVideoInterlace_Progressive : MTBest.MF_MT_INTERLACE_MODE)) &&
								SUCCEEDED(MFSetAttributeRatio(mediaTypeOut.Get(), MF_MT_PIXEL_ASPECT_RATIO, aspectRatioN, aspectRatioD)) &&
								SUCCEEDED(MFSetAttributeSize(mediaTypeOut.Get(), MF_MT_FRAME_SIZE, MTBest.width, MTBest.height)) &&
								SUCCEEDED(mediaTypeOut->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, convertToFormat ? 1 : MTBest.MF_MT_FIXED_SIZE_SAMPLES)) &&
								SUCCEEDED(mediaTypeOut->SetUINT32(MF_MT_SAMPLE_SIZE, convertToFormat ? outSize : MTBest.MF_MT_SAMPLE_SIZE)) &&
								SUCCEEDED(mediaTypeOut->SetUINT32(MF_MT_DEFAULT_STRIDE, convertToFormat ? outStride : MTBest.MF_MT_DEFAULT_STRIDE)))//Assume BGR24 input
							{
								if (SUCCEEDED(videoFileSource->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, false)) &&
									SUCCEEDED(videoFileSource->SetStreamSelection((DWORD)dwStreamBest, true)) &&
									SUCCEEDED(videoFileSource->SetCurrentMediaType((DWORD)dwStreamBest, NULL, mediaTypeOut.Get()))
									)
								{
									dwStreamIndex = (DWORD)dwStreamBest;
									nativeFormat = MTBest;
									aspectN = aspectRatioN;
									aspectD = aspectRatioD;
									outputFormat = outFormat;
									convertFormat = convertToFormat;
									captureFormat = MediaType(mediaTypeOut.Get());
									// Success !
									double fps = getFramerate(nativeFormat); // MT.MF_MT_FRAME_RATE_DENOMINATOR != 0 ? ((double)MT.MF_MT_FRAME_RATE_NUMERATOR) / ((double)MT.MF_MT_FRAME_RATE_DENOMINATOR) : 0
									frameStep = (LONGLONG)(fps > 0 ? 1e7 / fps : 0);
								}
								close();
							}
						}
					}
				}
				ppDevices[i]->Release();
			}
		}
	}
	CoTaskMemFree(ppDevices);
	/* Update device data. */
	return opened ? 0 : 1;
}


void sr_webcam_start(sr_webcam_device * device) {
	if (device->stream) {
	}
}

void sr_webcam_stop(sr_webcam_device * device) {
	if (device->stream) {
		
	}
}

#endif
