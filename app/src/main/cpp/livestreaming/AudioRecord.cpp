//
// Created by root on 2021/9/2.
//

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

#include "include/Log.h"
#include "MediaCodec.h"
#include "AudioRecord.h"
#include "MyJni.h"

#define LOG "player_alexander"

#define RECORDER_FRAMES 4096

static pthread_mutex_t audioEngineLock = PTHREAD_MUTEX_INITIALIZER;

// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;

static SLObjectItf recorderObject = NULL;
static SLRecordItf recorderRecord;
static SLAndroidSimpleBufferQueueItf recorderBufferQueue;

AudioRecord::AudioRecord() :
//engineObject(NULL),
//engineEngine(NULL),
//recorderObject(NULL),
//recorderRecord(NULL),
//recorderBufferQueue(NULL),
        index(0),
        recordBufferSize(RECORDER_FRAMES),
        _isDoing(false),
        mediaCodec(NULL) {
    LOGI("AudioRecord::AudioRecord()");
    //recordBuffers[0] = (unsigned char *) malloc(recordBufferSize * sizeof(unsigned char));
    //recordBuffers[1] = (unsigned char *) malloc(recordBufferSize * sizeof(unsigned char));
}

AudioRecord::~AudioRecord() {
    LOGI("AudioRecord::~AudioRecord()");
    if (recordBuffers[0]) {
        //delete recordBuffers[0];
        free(recordBuffers[0]);
        recordBuffers[0] = NULL;
    }
    LOGI("AudioRecord::~AudioRecord() 1");
    if (recordBuffers[1]) {
        //delete recordBuffers[1];
        free(recordBuffers[0]);
        recordBuffers[1] = NULL;
    }
    LOGI("AudioRecord::~AudioRecord() 2");
    // destroy audio recorder object, and invalidate all associated interfaces
    if (recorderObject != NULL) {
        (*recorderObject)->Destroy(recorderObject);
        recorderObject = NULL;
        recorderRecord = NULL;
        recorderBufferQueue = NULL;
    }
    LOGI("AudioRecord::~AudioRecord() 3");
    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }
    LOGI("AudioRecord::~AudioRecord() 4");
    //pthread_mutex_destroy(&audioEngineLock);
}

void AudioRecord::setBufferSize(int size) {
    LOGI("AudioRecord::setBufferSize() size: %d", size);
    recordBufferSize = size;
    recordBuffers[0] = (unsigned char *) malloc(recordBufferSize * sizeof(unsigned char));
    recordBuffers[1] = (unsigned char *) malloc(recordBufferSize * sizeof(unsigned char));
}

void AudioRecord::createEngine() {
    SLresult result;

    // create engine ??????????????????????????????????????????(OpenSL ES????????????)
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the engine ?????????????????????
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the engine interface, which is needed in order to create other objects ???????????????????????????????????????
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

void AudioRecord::createAudioRecorder() {
    LOGI("AudioRecord::createAudioRecorder()");
    SLresult result;

    // configure audio source ??????IO??????(?????????)
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE,      //??????
                                      SL_IODEVICE_AUDIOINPUT,       //device?????? ???????????????????????????
                                      SL_DEFAULTDEVICEID_AUDIOINPUT,//deviceID
                                      NULL};                        //device??????
    SLDataSource audioSource = {&loc_dev, NULL};

    // configure audio sink ????????????buffer??????
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,//?????? ???????????????????????????
            2};                         //buffer?????????
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,          //??????PCM???????????????
                                   2,             //?????????????????????
                                   SL_SAMPLINGRATE_44_1,       //?????????????????????????????????44100Hz
                                   SL_PCMSAMPLEFORMAT_FIXED_16,//?????????????????????????????????16bit
                                   SL_PCMSAMPLEFORMAT_FIXED_16,//????????????????????????????????????
            //?????????????????????????????????????????? SL_SPEAKER_FRONT_CENTER
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                   SL_BYTEORDER_LITTLEENDIAN}; //PCM????????????????????????
    SLDataSink audioSink = {&loc_bq, &format_pcm};

    // create audio recorder ?????????????????????
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioRecorder(
            engineEngine,   //????????????
            &recorderObject,//???????????????????????????????????????
            &audioSource,   //????????????
            &audioSink,     //????????????
            1,              //?????????????????????
            id,             //????????????????????????
            req);           //???????????????????????????????????????????????????
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    // realize the audio recorder ???????????????????????????
    result = (*recorderObject)->Realize(
            recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    // get the record interface ??????????????????
    result = (*recorderObject)->GetInterface(
            recorderObject, SL_IID_RECORD, &recorderRecord);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the buffer queue interface ??????Buffer??????
    result = (*recorderObject)->GetInterface(
            recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &recorderBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // register callback on the buffer queue
    result = (*recorderBufferQueue)->RegisterCallback(
            recorderBufferQueue, bqRecorderCallback, this);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

void AudioRecord::startRecording() {
    LOGI("AudioRecord::startRecording() start\n");
    _isDoing = true;
    SLresult result;

    if (pthread_mutex_trylock(&audioEngineLock)) {
        return;
    }

    // in case already recording, stop recording and clear buffer queue
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_STOPPED);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    result = (*recorderBufferQueue)->Clear(recorderBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // the buffer is not valid for playback yet
    //recorderSize = 0;

    // enqueue an empty buffer to be filled by the recorder
    // (for streaming recording, we would enqueue at least 2 empty buffers to start things off)
    result = (*recorderBufferQueue)->Enqueue(
            recorderBufferQueue, recordBuffers[index], recordBufferSize);
    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
    // which for this code example would indicate a programming error
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // start recording ????????????
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    LOGI("AudioRecord::startRecording() end\n");
}

// this callback handler is called every time a buffer finishes recording
void AudioRecord::bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    if (context == NULL) {
        return;
    }

    AudioRecord *audioRecord = (AudioRecord *) context;
    bool needStop = false;
    // for streaming recording, here we would call Enqueue to give recorder the next buffer to fill
    // but instead, this is a one-time buffer so we stop recording
    SLresult result;
    if (audioRecord->_isDoing) {
        int tempIndex = audioRecord->index;
        unsigned int tempRecordBufferSize = audioRecord->recordBufferSize;

        audioRecord->index = 1 - audioRecord->index;
        (*recorderBufferQueue)->Enqueue(recorderBufferQueue,
                                        audioRecord->recordBuffers[audioRecord->index],
                                        audioRecord->recordBufferSize);
        /*(*audioRecord->recorderBufferQueue)->Enqueue(audioRecord->recorderBufferQueue,
                                                     audioRecord->recordBuffers[audioRecord->index],
                                                     audioRecord->recordBufferSize);*/

        // ??????PCM??????(????????????)
        if (audioRecord->mediaCodec != NULL) {
            /*LOGI("AudioRecord::bqRecorderCallback() tempRecordBufferSize: %lld\n",
                 audioRecord->recordBufferSize);*/
            bool feedAndDrain = audioRecord->mediaCodec->feedInputBufferAndDrainOutputBuffer(
                    audioRecord->mediaCodec->getMediaCodec(),
                    audioRecord->recordBuffers[tempIndex],
                    0,
                    tempRecordBufferSize,
                    0,//
                    0,
                    true, false, true);
            if (!feedAndDrain) {
                needStop = true;
            }
        }
    } else {
        needStop = true;
    }

    if (needStop) {
        audioRecord->_isDoing = false;
        LOGI("AudioRecord::bqRecorderCallback() SL_RECORDSTATE_STOPPED\n");
        result = (*recorderRecord)->SetRecordState(
                recorderRecord, SL_RECORDSTATE_STOPPED);
        /*result = (*audioRecord->recorderRecord)->SetRecordState(
                audioRecord->recorderRecord, SL_RECORDSTATE_STOPPED);*/
        if (SL_RESULT_SUCCESS == result) {
            //recorderSize = RECORDER_FRAMES * sizeof(short);
            LOGI("AudioRecord::bqRecorderCallback() SL_RESULT_SUCCESS\n");
        }
        onTransact_release(nullptr, nullptr, 0, nullptr);
    }

    pthread_mutex_unlock(&audioEngineLock);
}
