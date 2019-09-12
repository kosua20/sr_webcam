#include "sr_webcam_internal.h"

#if defined(__OBJC__)

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <Accelerate/Accelerate.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

@interface VideoStreamAVFoundation : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate> {
	
	AVCaptureSession *_captureSession;
	AVCaptureDevice *_captureDevice;
	AVCaptureVideoDataOutput *_captureDataOut;
	AVCaptureDeviceInput *_captureDataIn;
	
	@public sr_webcam_device * _parent;
	
	@public int _width;
	@public int _height;
	@public int _id;
}

-(BOOL)setupWithRate:(int)framerate width:(int)w height:(int)h;
-(void)start;
-(void)stop;


@end

@implementation VideoStreamAVFoundation

- (id)init {
	self = [super init];
	if (self) {
		_captureDataIn = nil;
		_captureDataOut = nil;
		_captureDevice = nil;
		
		_parent = NULL;
		_width = 0;
		_height = 0;
		_id = 0;
	}
	return self;
}

-(BOOL)setupWithRate:(int)framerate width:(int)w height:(int)h {
	// List available devices
	NSArray * devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
	if([devices count] == 0) {
		return NO;
	}
	_id = MAX(_id, [devices count]-1);
	_captureDevice = [devices objectAtIndex:_id];
	
	// Setup the device.
	NSError *err = nil;
	[_captureDevice lockForConfiguration:&err];
	if(err){
		return NO;
	}
	// Look for the best format.
	AVCaptureDeviceFormat * bestFormat = nil;
	int wBest = 0;
	int hBest = 0;
	float bestFit = 1e9f;
	for(AVCaptureDeviceFormat * format in [_captureDevice formats] ) {
		CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
		const int wFormat = dimensions.width;
		const int hFormat = dimensions.height;
		// Found the perfect mode.
		if(wFormat == w && hFormat == h){
			wBest = wFormat;
			hBest = hFormat;
			bestFormat = format;
			break;
		}
		const float dw = (float)(w - wFormat);
		const float dh = (float)(h - hFormat);
		const float fit = sqrtf(dw*dw+dh*dh);
		if(fit < bestFit){
			bestFit = fit;
			wBest = wFormat;
			hBest = hFormat;
			bestFormat = format;
		}
	}
	
	// If we found a valid format, set it.
	if(bestFormat){
		[_captureDevice setActiveFormat:bestFormat];
		_width = wBest;
		_height = hBest;
	}
	// Now try to adjust the framerate.
	if(framerate > 0){
		NSArray * validRates = _captureDevice.activeFormat.videoSupportedFrameRateRanges;
		AVFrameRateRange * bestRange = [AVFrameRateRange alloc];
		BOOL found = NO;
		for(AVFrameRateRange * range in validRates){
			if(framerate >= floor([range minFrameRate]) && framerate <= ceil([range maxFrameRate])){
				bestRange = range;
				found = YES;
			}
		}
		if(found){
			_captureDevice.activeVideoMinFrameDuration = bestRange.minFrameDuration;
			_captureDevice.activeVideoMaxFrameDuration = bestRange.maxFrameDuration;
		}
	}
	// Configuration done.
	[_captureDevice unlockForConfiguration];
	
	// Then, the device is ready.
	_captureDataIn = [AVCaptureDeviceInput deviceInputWithDevice:_captureDevice error:nil];
	_captureDataOut = [[AVCaptureVideoDataOutput alloc] init];
	_captureDataOut.alwaysDiscardsLateVideoFrames = YES;
	
	// We receive data on a secondary thread.
	dispatch_queue_t queue;
	queue = dispatch_queue_create("VideoStream", DISPATCH_QUEUE_SERIAL);
	dispatch_set_target_queue( queue, dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_HIGH, 0 ) );
	[_captureDataOut setSampleBufferDelegate:self queue:queue];
	// Not sure, OF does this but not the Apple sample.
	//dispatch_release(queue);
	
	// Create video settings for the output, corresponding to the current format.
	NSDictionary * settings = [NSDictionary dictionaryWithObjectsAndKeys:
							   [NSNumber numberWithUnsignedInt: kCVPixelFormatType_32BGRA], kCVPixelBufferPixelFormatTypeKey,
							   [NSNumber numberWithDouble: _width], kCVPixelBufferWidthKey,
							   [NSNumber numberWithDouble: _height], kCVPixelBufferHeightKey,
							   nil];
	[_captureDataOut setVideoSettings:settings];
	
	_captureSession = [[[AVCaptureSession alloc] init] autorelease];
	[_captureSession beginConfiguration];
	[_captureSession addInput:_captureDataIn];
	[_captureSession addOutput:_captureDataOut];
	// Setup output (once added), limit acquisition frequency.
	AVCaptureConnection * connection = [_captureDataOut connectionWithMediaType:AVMediaTypeVideo];
	if([connection isVideoMinFrameDurationSupported] == YES){
		[connection setVideoMinFrameDuration:CMTimeMake(1, framerate)];
	}
	if([connection isVideoMaxFrameDurationSupported] == YES){
		[connection setVideoMaxFrameDuration:CMTimeMake(1, framerate)];
	}
	[_captureSession commitConfiguration];
	//[_captureSession startRunning];
	
	return YES;
}

-(void)start {
	[_captureSession startRunning];
	[_captureDataIn.device lockForConfiguration:nil];
	// Enable autofocus.
	if([_captureDataIn.device isFocusModeSupported:AVCaptureFocusModeAutoFocus]){
	//	[_captureDataIn.device setFocusMode:AVCaptureFocusModeAutoFocus];
	}
}

-(void)stop {
	if(_captureSession){
		if(_captureDataOut){
			// Remove the delegate.
			if(_captureDataOut.sampleBufferDelegate != nil) {
				[_captureDataOut setSampleBufferDelegate:nil queue:NULL];
			}
		}
		// Remove the input and output.
		for(AVCaptureInput * input in _captureSession.inputs){
			[_captureSession removeInput:input];
		}
		for(AVCaptureOutput * output in _captureSession.outputs){
			[_captureSession removeOutput:output];
		}
		[_captureSession stopRunning];
	}
}


/// Implement the delegate.

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer  fromConnection:(AVCaptureConnection *)connection {
	if(!_parent){
		return;
	}
	@autoreleasepool {
		CVImageBufferRef imgBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
		CVPixelBufferLockBaseAddress(imgBuffer,0);
		unsigned char *baseBuffer = (unsigned char *)CVPixelBufferGetBaseAddress(imgBuffer);
		size_t wBuffer  = CVPixelBufferGetWidth(imgBuffer);
		size_t hBuffer = CVPixelBufferGetHeight(imgBuffer);
		if(wBuffer == _parent->width && hBuffer == _parent->height){
			// Pass the data.
			_parent->callback(_parent, baseBuffer);
		}
		CVPixelBufferUnlockBaseAddress(imgBuffer, kCVPixelBufferLock_ReadOnly);
	}
	
}


- (void)dealloc {
	if(_captureSession) {
		[self stop];
		_captureSession = nil;
	}
	if(_captureDataOut){
		if(_captureDataOut.sampleBufferDelegate != nil) {
			[_captureDataOut setSampleBufferDelegate:nil queue:NULL];
		}
		[_captureDataOut release];
		_captureDataOut = nil;
	}
	if(_parent){
		_parent = NULL;
	}
	_captureDataIn = nil;
	_captureDevice = nil;
	[super dealloc];
}

@end


int sr_webcam_setup(sr_webcam_device * device){
	VideoStreamAVFoundation * stream = [VideoStreamAVFoundation alloc];
	device->stream = stream;
	stream->_parent = device;
	BOOL res = [stream setupWithRate:device->framerate width:device->width height:device->height];
	if(res == NO){
		return -1;
	}
	device->width = stream->_width;
	device->height = stream->_height;
	device->deviceId = stream->_id;
	return 0;
}


void sr_webcam_start(sr_webcam_device * device){
	if(device->stream){
		VideoStreamAVFoundation * stream = (VideoStreamAVFoundation*)(device->stream);
		[stream start];
	}
}

void sr_webcam_stop(sr_webcam_device * device){
	if(device->stream){
		VideoStreamAVFoundation * stream = (VideoStreamAVFoundation*)(device->stream);
		[stream stop];
	}
}

#endif
