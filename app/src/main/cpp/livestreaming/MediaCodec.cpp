//
// Created by root on 2021/8/11.
//

#include <pthread.h>
#include <assert.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include "include/Log.h"
#include "MyJni.h"
#include "MediaCodec.h"

#define LOG "player_alexander"

static int TIME_OUT = 10000;

MediaCodec::MediaCodec() :
        _surface(nullptr),
        _format(nullptr),
        _codec(nullptr),
        _mime(nullptr),
        _mime_length(0),
        _codec_name(nullptr),
        _codec_name_length(0),
        _sps_pps(nullptr),
        _sps(nullptr),
        _pps(nullptr),
        _sps_pps_length(0),
        _sps_length(0),
        _pps_length(0),
        _width(0),
        _height(0),
        _isDoing(false),
        _callback(nullptr) {
    LOGI("MediaCodec::MediaCodec()");
}

MediaCodec::~MediaCodec() {
    LOGI("MediaCodec::~MediaCodec()");
    if (_surface) {
        ANativeWindow_release(_surface);
        _surface = nullptr;
    }
    if (_format) {
        AMediaFormat_delete(_format);
        _format = nullptr;
    }
    if (_codec) {
        AMediaCodec_delete(_codec);
        _codec = nullptr;
    }
    if (_sps_pps) {
        free(_sps_pps);
        _sps_pps = nullptr;
        _sps_pps_length = 0;
    }
    _callback = nullptr;
}

/*

 */
/*void setParameters(JNIEnv *env, jobject surface_obj,
                               const char *mime, const char *codec_name,
                               uint8_t *sps_pps, int sps_pps_length,
                               int width, int height) {
    if (surface_obj != nullptr) {
        _surface = ANativeWindow_fromSurface(env, surface_obj);
    }
    if (codec_name != nullptr) {
        _codec = AMediaCodec_createCodecByName(codec_name);
    }
    AMediaFormat *pFormat = AMediaFormat_new();
    AMediaFormat_setString(pFormat, AMEDIAFORMAT_KEY_MIME, mime);
    if (width > 0 && height > 0) {
        AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_WIDTH, width);
        AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_HEIGHT, height);
        AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_MAX_WIDTH, width);
        AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_MAX_HEIGHT, height);
        AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, width * height);
    }
    if (strcmp(mime, "video/hevc") == 0) {
        if (sps_pps_length > 0) {
            AMediaFormat_setBuffer(pFormat, "csd-0", sps_pps, sps_pps_length);
        }
    } else if (strcmp(mime, "video/avc") == 0) {
        int mark = 0;
        if (sps_pps_length > 0) {
            mark = handleSpsPps(pFormat, sps_pps, sps_pps_length);
            if (mark == 0) {
                LOGE("MediaCodec::setParams() video/avc 找不到相关的sps pps数据");
            }
        }
    }
    _format = pFormat;
}*/

void MediaCodec::startScreenRecordEncoderMediaCodec(
        const char *mime,
        const char *codec_name,
        int width, int height) {
    if (mime == nullptr || codec_name == nullptr) {
        return;
    }

    AMediaFormat *pFormat = AMediaFormat_new();
    AMediaFormat_setString(pFormat, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_HEIGHT, height);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_MAX_WIDTH, width);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_MAX_HEIGHT, height);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, width * height);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x7F000789);// 录制屏幕专用
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_BIT_RATE, 10000000);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_FRAME_RATE, 25);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 1);
    _format = pFormat;

    _codec = AMediaCodec_createCodecByName(codec_name);
    AMediaCodec_configure(_codec, _format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);

    // #if __ANDROID_API__ >= 26
    AMediaCodec_createInputSurface(_codec, &_surface);

    AMediaCodec_start(_codec);
}

void MediaCodec::startScreenRecord() {
    if (_isDoing) {
        LOGE("startScreenRecord() return for _isDoing is true\n");
        return;
    }

    LOGI("startScreenRecord() start\n");
    _isDoing = true;

    createPortraitVirtualDisplay();
    if (_sps_pps == nullptr) {
        getSpsPps();
    }

    // 开启线程不断地读取数据
    pthread_t p_tids_receive_data;
    // 定义一个属性
    pthread_attr_t attr;
    sched_param param;
    // 初始化属性值,均设为默认值
    pthread_attr_init(&attr);
    pthread_attr_getschedparam(&attr, &param);
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&p_tids_receive_data, &attr, startEncoder, this);
    LOGI("startScreenRecord() end\n");
}

void MediaCodec::startAudioRecordEncoderMediaCodec(
        const char *mime, const char *codec_name,
        int sampleRate, int channelCount, int channelConfig, int maxInputSize) {
    if (mime == nullptr || codec_name == nullptr) {
        return;
    }

    AMediaFormat *pFormat = AMediaFormat_new();
    AMediaFormat_setString(pFormat, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, sampleRate);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, channelCount);
    // 2为MediaCodecInfo.CodecProfileLevel.AACObjectLC
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_AAC_PROFILE, 2);
    // 2为AudioFormat.ENCODING_PCM_16BIT
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_BIT_RATE, sampleRate * channelCount * 2);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_CHANNEL_MASK, channelConfig);
    AMediaFormat_setInt32(pFormat, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, maxInputSize);
    _format = pFormat;

    _codec = AMediaCodec_createCodecByName(codec_name);
    AMediaCodec_configure(_codec, _format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);

    AMediaCodec_start(_codec);
}

///////////////////////////////////////////////////////////////////////////////////////////

void *MediaCodec::startEncoder(void *arg) {
    MediaCodec *mediaCodec = (MediaCodec *) arg;

    LOGI("startEncoder() start\n");
    while (mediaCodec->_isDoing) {
        mediaCodec->drainOutputBuffer(mediaCodec->_codec, true, false);
    }
    LOGI("startEncoder() end\n");
}

bool MediaCodec::feedInputBufferAndDrainOutputBuffer(AMediaCodec *codec,
                                                     unsigned char *data,
                                                     off_t offset, size_t size,
                                                     uint64_t time, uint32_t flags,
                                                     bool release,
                                                     bool render,
                                                     bool needFeedInputBuffer) {
    if (needFeedInputBuffer) {
        return feedInputBuffer(codec, data, offset, size, time, flags) &&
               drainOutputBuffer(codec, release, render);
    }

    return drainOutputBuffer(codec, release, render);
}

bool MediaCodec::feedInputBuffer(AMediaCodec *codec,
                                 unsigned char *data, off_t offset, size_t size,
                                 uint64_t time, uint32_t flags) {
    ssize_t roomIndex = AMediaCodec_dequeueInputBuffer(codec, TIME_OUT);
    if (roomIndex < 0) {
        return true;
    }

    size_t out_size = 0;
    //auto room = AMediaCodec_getInputBuffer(codec, roomIndex, &out_size);
    uint8_t *room = AMediaCodec_getInputBuffer(codec, (size_t) roomIndex, &out_size);
    if (room == nullptr) {
        return false;
    }
    memcpy(room, data, size);
    AMediaCodec_queueInputBuffer(codec, roomIndex, offset, size, time, flags);
    return true;
}

bool MediaCodec::drainOutputBuffer(AMediaCodec *codec, bool release, bool render) {
    AMediaCodecBufferInfo info;
    size_t out_size = 0;
    for (;;) {
        if (!_isDoing) {
            break;
        }

        ssize_t roomIndex = AMediaCodec_dequeueOutputBuffer(codec, &info, TIME_OUT);
        if (roomIndex < 0) {
            switch (roomIndex) {
                case AMEDIACODEC_INFO_TRY_AGAIN_LATER: {
                    break;
                }
                case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED: {
                    auto format = AMediaCodec_getOutputFormat(codec);
                    LOGI("format changed to: %s", AMediaFormat_toString(format));
                    AMediaFormat_delete(format);
                    break;
                }
                case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED: {
                    break;
                }
                default:
                    break;
            }
            break;
        }

        /*if (info.flags & 1) {
            LOGI("info.flags 1: %d", info.flags);// 关键帧
        }
        if (info.flags & 2) {
            LOGI("info.flags 2: %d", info.flags);// 配置帧
        }
        //AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM
        if (info.flags & 4) {
            LOGI("info.flags 4: %d", info.flags);
        }*/

        uint8_t *room = AMediaCodec_getOutputBuffer(codec, (size_t) roomIndex, &out_size);
        if (room == nullptr) {
            AMediaCodec_releaseOutputBuffer(codec, roomIndex, render);
            return false;
        }

        // 处理数据
        if (_callback) {
            _callback(info, room);
        }

        if (release) {
            AMediaCodec_releaseOutputBuffer(codec, roomIndex, render);
        }
    }

    return true;
}

int MediaCodec::handleSpsPps(AMediaFormat *pFormat, uint8_t *sps_pps, ssize_t size) {
    // unknow
    static int MARK0 = 0;
    // 0 0 0 1
    static int MARK1 = 1;
    //   0 0 1
    static int MARK2 = 2;
    // ... 103 ... 104 ...
    static int MARK3 = 3;
    // ...  39 ... 40 ...
    static int MARK4 = 4;

    uint8_t *sps = nullptr;
    uint8_t *pps = nullptr;
    size_t sps_size = 0;
    size_t pps_size = 0;
    int mark = MARK0;
    int index = -1;
    if (sps_pps[0] == 0
        && sps_pps[1] == 0
        && sps_pps[2] == 0
        && sps_pps[3] == 1) {
        // region 0 0 0 1 ... 0 0 0 1 ...
        for (int i = 1; i < size; i++) {
            if (sps_pps[i] == 0
                && sps_pps[i + 1] == 0
                && sps_pps[i + 2] == 0
                && sps_pps[i + 3] == 1) {
                index = i;
                mark = MARK1;
                break;
            }
        }
        // endregion
    } else if (sps_pps[0] == 0
               && sps_pps[1] == 0
               && sps_pps[2] == 1) {
        // region 0 0 1 ... 0 0 1 ...
        for (int i = 1; i < size; i++) {
            if (sps_pps[i] == 0
                && sps_pps[i + 1] == 0
                && sps_pps[i + 2] == 1) {
                index = i;
                mark = MARK2;
                break;
            }
        }
        // endregion
    }

    if (index != -1) {
        // region
        sps_size = index;
        pps_size = size - index;
        sps = (uint8_t *) malloc(sps_size);
        pps = (uint8_t *) malloc(pps_size);
        memset(sps, 0, sps_size);
        memset(pps, 0, pps_size);
        memcpy(sps, sps_pps, sps_size);
        memcpy(pps, sps_pps + index, pps_size);
        // endregion
    } else {
        // region ... 103 ... 104 ...
        mark = MARK3;
        int spsIndex = -1;
        int spsLength = 0;
        int ppsIndex = -1;
        int ppsLength = 0;
        for (int i = 0; i < size; i++) {
            if (sps_pps[i] == 103) {
                // 0x67 = 103
                if (spsIndex == -1) {
                    spsIndex = i;
                    spsLength = sps_pps[i - 1];
                    if (spsLength <= 0) {
                        spsIndex = -1;
                    }
                }
            } else if (sps_pps[i] == 104) {
                // 103后面可能有2个或多个104
                // 0x68 = 104
                ppsIndex = i;
                ppsLength = sps_pps[i - 1];
            }
        }
        // endregion

        // region ... 39 ... 40 ...
        if (spsIndex == -1 || ppsIndex == -1) {
            mark = MARK4;
            spsIndex = -1;
            spsLength = 0;
            ppsIndex = -1;
            ppsLength = 0;
            for (int i = 0; i < size; i++) {
                if (sps_pps[i] == 39) {
                    if (spsIndex == -1) {
                        spsIndex = i;
                        spsLength = sps_pps[i - 1];
                        if (spsLength <= 0) {
                            spsIndex = -1;
                        }
                    }
                } else if (sps_pps[i] == 40) {
                    ppsIndex = i;
                    ppsLength = sps_pps[i - 1];
                }
            }
        }
        // endregion

        // region
        if (spsIndex != -1 && ppsIndex != -1) {
            sps_size = spsLength;
            pps_size = ppsLength;
            sps = (uint8_t *) malloc(spsLength + 4);
            pps = (uint8_t *) malloc(ppsLength + 4);
            memset(sps, 0, spsLength + 4);
            memset(pps, 0, ppsLength + 4);

            // 0x00, 0x00, 0x00, 0x01
            sps[0] = pps[0] = 0;
            sps[1] = pps[1] = 0;
            sps[2] = pps[2] = 0;
            sps[3] = pps[3] = 1;
            memcpy(sps + 4, sps_pps + spsIndex, spsLength);
            memcpy(pps + 4, sps_pps + ppsIndex, ppsLength);
        }
        // endregion
    }

    if (sps != nullptr && pps != nullptr) {
        AMediaFormat_setBuffer(pFormat, "csd-0", sps, sps_size);
        AMediaFormat_setBuffer(pFormat, "csd-1", pps, pps_size);
    } else {
        // 实在找不到sps和pps的数据了
        mark = MARK0;
    }

    return mark;
}

int MediaCodec::handleSpsPps(uint8_t *sps_pps, ssize_t size) {
    // unknow
    static int MARK0 = 0;
    // 0 0 0 1
    static int MARK1 = 1;
    //   0 0 1
    static int MARK2 = 2;
    // ... 103 ... 104 ...
    static int MARK3 = 3;
    // ...  39 ... 40 ...
    static int MARK4 = 4;

    //uint8_t *_sps = nullptr;
    //uint8_t *_pps = nullptr;
    //size_t _sps_length = 0;
    //size_t _pps_length = 0;
    int mark = MARK0;
    int index = -1;
    if (sps_pps[0] == 0
        && sps_pps[1] == 0
        && sps_pps[2] == 0
        && sps_pps[3] == 1) {
        // region 0 0 0 1 ... 0 0 0 1 ...
        for (int i = 1; i < size; i++) {
            if (sps_pps[i] == 0
                && sps_pps[i + 1] == 0
                && sps_pps[i + 2] == 0
                && sps_pps[i + 3] == 1) {
                index = i;
                mark = MARK1;
                break;
            }
        }
        // endregion
    } else if (sps_pps[0] == 0
               && sps_pps[1] == 0
               && sps_pps[2] == 1) {
        // region 0 0 1 ... 0 0 1 ...
        for (int i = 1; i < size; i++) {
            if (sps_pps[i] == 0
                && sps_pps[i + 1] == 0
                && sps_pps[i + 2] == 1) {
                index = i;
                mark = MARK2;
                break;
            }
        }
        // endregion
    }

    if (index != -1) {
        // region
        _sps_length = index;
        _pps_length = size - index;
        _sps = (uint8_t *) malloc(_sps_length);
        _pps = (uint8_t *) malloc(_pps_length);
        memset(_sps, 0, _sps_length);
        memset(_pps, 0, _pps_length);
        memcpy(_sps, sps_pps, _sps_length);
        memcpy(_pps, sps_pps + index, _pps_length);
        // endregion
    } else {
        // region ... 103 ... 104 ...
        mark = MARK3;
        int spsIndex = -1;
        int spsLength = 0;
        int ppsIndex = -1;
        int ppsLength = 0;
        for (int i = 0; i < size; i++) {
            if (sps_pps[i] == 103) {
                // 0x67 = 103
                if (spsIndex == -1) {
                    spsIndex = i;
                    spsLength = sps_pps[i - 1];
                    if (spsLength <= 0) {
                        spsIndex = -1;
                    }
                }
            } else if (sps_pps[i] == 104) {
                // 103后面可能有2个或多个104
                // 0x68 = 104
                ppsIndex = i;
                ppsLength = sps_pps[i - 1];
            }
        }
        // endregion

        // region ... 39 ... 40 ...
        if (spsIndex == -1 || ppsIndex == -1) {
            mark = MARK4;
            spsIndex = -1;
            spsLength = 0;
            ppsIndex = -1;
            ppsLength = 0;
            for (int i = 0; i < size; i++) {
                if (sps_pps[i] == 39) {
                    if (spsIndex == -1) {
                        spsIndex = i;
                        spsLength = sps_pps[i - 1];
                        if (spsLength <= 0) {
                            spsIndex = -1;
                        }
                    }
                } else if (sps_pps[i] == 40) {
                    ppsIndex = i;
                    ppsLength = sps_pps[i - 1];
                }
            }
        }
        // endregion

        // region
        if (spsIndex != -1 && ppsIndex != -1) {
            _sps_length = spsLength;
            _pps_length = ppsLength;
            _sps = (uint8_t *) malloc(spsLength + 4);
            _pps = (uint8_t *) malloc(ppsLength + 4);
            memset(_sps, 0, spsLength + 4);
            memset(_pps, 0, ppsLength + 4);

            // 0x00, 0x00, 0x00, 0x01
            _sps[0] = _pps[0] = 0x00;
            _sps[1] = _pps[1] = 0x00;
            _sps[2] = _pps[2] = 0x00;
            _sps[3] = _pps[3] = 0x01;
            memcpy(_sps + 4, sps_pps + spsIndex, spsLength);
            memcpy(_pps + 4, sps_pps + ppsIndex, ppsLength);
        }
        // endregion
    }

    if (_sps != nullptr && _pps != nullptr) {

    } else {
        // 实在找不到sps和pps的数据了
        mark = MARK0;
    }

    return mark;
}

void MediaCodec::getSpsPps() {
    bool isGettingSpsPps = true;
    AMediaCodecBufferInfo roomInfo;
    size_t out_size = 0;
    LOGI("getSpsPps() start\n");
    while (isGettingSpsPps) {
        for (;;) {
            ssize_t roomIndex = AMediaCodec_dequeueOutputBuffer(_codec, &roomInfo, TIME_OUT);
            if (roomIndex < 0) {
                break;
            }
            uint8_t *room = AMediaCodec_getOutputBuffer(_codec, (size_t) roomIndex, &out_size);
            if (room != nullptr && (roomInfo.flags & 2)) {// 配置帧
                isGettingSpsPps = false;
                handleSpsPps(room, roomInfo.size);
                AMediaCodec_releaseOutputBuffer(_codec, roomIndex, false);
                break;
            }
            AMediaCodec_releaseOutputBuffer(_codec, roomIndex, false);
        }
    }
    LOGI("getSpsPps() end\n");
}




























































