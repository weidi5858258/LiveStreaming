//
// Created by root on 2021/8/11.
//

#ifndef MIRRORCAST_MEDIACODEC_H
#define MIRRORCAST_MEDIACODEC_H

#include "media/NdkMediaCodec.h"
#include "media/NdkMediaExtractor.h"

// for native surface JNI
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "Send.h"

class MediaCodec {
public:
    enum MARK {
        // unknow
        MARK0 = 0,
        // 0 0 0 1
        MARK1 = 1,
        //   0 0 1
        MARK2 = 2,
        // ... 103 ... 104 ...
        MARK3 = 3,
        // ...  39 ... 40 ...
        MARK4 = 4
    };
private:
    ANativeWindow *_surface = nullptr;
    AMediaFormat *_format;
    AMediaCodec *_codec;
    char *_mime;
    int _mime_length;
    char *_codec_name;
    int _codec_name_length;
    // 使用于h265或者不分开的h264
    uint8_t *_sps_pps;
    int _sps_pps_length;
    // 使用于分开的h264
    uint8_t *_sps;
    uint8_t *_pps;
    int _sps_length;
    int _pps_length;
    int _width;
    int _height;
    MARK _mark;

    bool _isDoing;
    bool _isVideo;
    Send *_send;
    RTMP *_rtmp;
public:
    MediaCodec();

    virtual ~MediaCodec();

    /*void setParameters(JNIEnv *env, jobject surface_obj,
                       const char *mime, const char *codec_name,
                       uint8_t *sps_pps, int sps_pps_length,
                       int width, int height);*/

    AMediaCodec *getMediaCodec() {
        return _codec;
    }

    bool feedInputBufferAndDrainOutputBuffer(AMediaCodec *codec,
                                             unsigned char *data,
                                             off_t offset, size_t size,
                                             uint64_t time, uint32_t flags,
                                             bool release,
                                             bool render,
                                             bool needFeedInputBuffer);

    void gameOver() {
        _isDoing = false;
    }

    /*void setCallBack(void (*callback)(MARK mark, AMediaCodecBufferInfo &, uint8_t *)) {
        _callback = callback;
    }*/

    ANativeWindow *getScreenRecordSurface() {
        return _surface;
    }

    void startScreenRecordEncoderMediaCodec(
            const char *mime, const char *codec_name,
            int width, int height);

    void startScreenRecord();

    void startAudioRecordEncoderMediaCodec(
            const char *mime, const char *codec_name,
            int sampleRate, int channelCount, int channelConfig, int maxInputSize);

    void setSend(Send *send) {
        this->_send = send;
    }

    void setRTMP(RTMP *rtmp) {
        this->_rtmp = rtmp;
    }

private:
    MARK handleSpsPps(uint8_t *sps_pps, AMediaCodecBufferInfo &info);

    MARK handleSpsPps(AMediaFormat *pFormat, uint8_t *sps_pps, ssize_t size);

    void getSpsPps();

    bool feedInputBuffer(AMediaCodec *codec,
                         unsigned char *data, off_t offset, size_t size,
                         uint64_t time, uint32_t flags);

    bool drainOutputBuffer(AMediaCodec *codec, bool release, bool render);

    //void (*_callback)(MARK mark, AMediaCodecBufferInfo &info, uint8_t *data);

    static void *startScreenRecordEncoder(void *arg);

    void screenRecordCallback(AMediaCodecBufferInfo &info, uint8_t *data);

    void audioRecordCallback(AMediaCodecBufferInfo &info, uint8_t *data);
};

#endif //MIRRORCAST_MEDIACODEC_H