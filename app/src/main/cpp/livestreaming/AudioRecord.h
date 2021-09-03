//
// Created by root on 2021/9/2.
//

#ifndef LIVESTREAMING_AUDIORECORD_H
#define LIVESTREAMING_AUDIORECORD_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

class AudioRecord {
private:
    // engine interfaces
    SLObjectItf engineObject;
    SLEngineItf engineEngine;

    SLObjectItf recorderObject;
    SLRecordItf recorderRecord;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue;

    int index;
    short *recordBuffers[2];
    unsigned int recordBufferSize;
    bool isRecording;

    MediaCodec *mediaCodec;
public:
    AudioRecord();

    virtual ~AudioRecord();

    void createEngine();

    void createAudioRecorder();

    void startRecording();

    void gameOver() {
        isRecording = false;
    }

    void setMediaCodec(MediaCodec *codec) {
        mediaCodec = codec;
    }

private:
    // 回调函数要设置成static
    static void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

};


#endif //LIVESTREAMING_AUDIORECORD_H
