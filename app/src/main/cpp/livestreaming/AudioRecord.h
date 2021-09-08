//
// Created by root on 2021/9/2.
//

#ifndef LIVESTREAMING_AUDIORECORD_H
#define LIVESTREAMING_AUDIORECORD_H

extern "C" {
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
};

class AudioRecord {
private:
    // engine interfaces
    SLObjectItf engineObject = NULL;
    SLEngineItf engineEngine;

    SLObjectItf recorderObject = NULL;
    SLRecordItf recorderRecord;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue;

    int index;
    unsigned char *recordBuffers[2];
    unsigned int recordBufferSize;

    bool _isDoing;

    MediaCodec *mediaCodec;
public:
    AudioRecord();

    virtual ~AudioRecord();

    void createEngine();

    void createAudioRecorder();

    void startRecording();

    void stopRecording() {
        _isDoing = false;
    }

    void gameOver() {
        _isDoing = false;
    }

    void setMediaCodec(MediaCodec *codec) {
        mediaCodec = codec;
    }

    void setBufferSize(int size);

private:
    // 回调函数要设置成static
    static void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

};


#endif //LIVESTREAMING_AUDIORECORD_H
