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

#define RECORDER_FRAMES 2048

static pthread_mutex_t audioEngineLock = PTHREAD_MUTEX_INITIALIZER;

AudioRecord::AudioRecord() :
        engineObject(NULL),
        engineEngine(NULL),
        recorderObject(NULL),
        recorderRecord(NULL),
        recorderBufferQueue(NULL),
        index(0),
        recordBufferSize(RECORDER_FRAMES),
        _isDoing(false),
        mediaCodec(NULL) {
    LOGI("AudioRecord::AudioRecord()");
    recordBuffers[0] = new short[recordBufferSize];
    recordBuffers[1] = new short[recordBufferSize];
}

AudioRecord::~AudioRecord() {
    LOGI("AudioRecord::~AudioRecord()");
    if (recordBuffers[0]) {
        delete recordBuffers[0];
        recordBuffers[0] = NULL;
    }

    if (recordBuffers[1]) {
        delete recordBuffers[1];
        recordBuffers[1] = NULL;
    }

    // destroy audio recorder object, and invalidate all associated interfaces
    if (recorderObject != NULL) {
        (*recorderObject)->Destroy(recorderObject);
        recorderObject = NULL;
        recorderRecord = NULL;
        recorderBufferQueue = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }

    pthread_mutex_destroy(&audioEngineLock);
}

void AudioRecord::createEngine() {
    SLresult result;

    // create engine 调用全局方法创建一个引擎对象(OpenSL ES唯一入口)
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the engine 实例化这个对象
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the engine interface, which is needed in order to create other objects 从这个对象里面获取引擎接口
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

void AudioRecord::createAudioRecorder() {
    LOGI("AudioRecord::createAudioRecorder()");
    SLresult result;

    // configure audio source 设置IO设备(麦克风)
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE,      //类型
                                      SL_IODEVICE_AUDIOINPUT,       //device类型 选择了音频输入类型
                                      SL_DEFAULTDEVICEID_AUDIOINPUT,//deviceID
                                      NULL};                        //device实例
    SLDataSource audioSource = {&loc_dev, NULL};

    // configure audio sink 设置输出buffer队列
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,//类型 这里只能是这个常量
            2};                         //buffer的数量
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,          //输出PCM格式的数据
                                   2,             //输出的声道数量
                                   SL_SAMPLINGRATE_44_1,       //输出的采样频率，这里是44100Hz
                                   SL_PCMSAMPLEFORMAT_FIXED_16,//输出的采样格式，这里是16bit
                                   SL_PCMSAMPLEFORMAT_FIXED_16,//一般来说，跟随上一个参数
            //双声道配置，如果单声道可以用 SL_SPEAKER_FRONT_CENTER
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                   SL_BYTEORDER_LITTLEENDIAN}; //PCM数据的大小端排列
    SLDataSink audioSink = {&loc_bq, &format_pcm};

    // create audio recorder 创建录制的对象
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioRecorder(
            engineEngine,   //引擎接口
            &recorderObject,//录制对象地址，用于传出对象
            &audioSource,   //输入配置
            &audioSink,     //输出配置
            1,              //支持的接口数量
            id,             //具体要支持的接口
            req);           //具体要支持的接口是开放的还是关闭的
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    // realize the audio recorder 实例化这个录制对象
    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    // get the record interface 获取录制接口
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the buffer queue interface 获取Buffer接口
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
            recorderBufferQueue,
            recordBuffers[index],
            recordBufferSize * sizeof(short));
    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
    // which for this code example would indicate a programming error
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // start recording 开始录音
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

// this callback handler is called every time a buffer finishes recording
void AudioRecord::bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    if (context == NULL) {
        return;
    }

    AudioRecord *audioRecord = (AudioRecord *) context;

    // for streaming recording, here we would call Enqueue to give recorder the next buffer to fill
    // but instead, this is a one-time buffer so we stop recording
    SLresult result;
    if (audioRecord->_isDoing) {
        int tempIndex = audioRecord->index;
        unsigned int tempRecordBufferSize = audioRecord->recordBufferSize;

        audioRecord->index = 1 - audioRecord->index;
        (*audioRecord->recorderBufferQueue)->Enqueue(audioRecord->recorderBufferQueue,
                                                     audioRecord->recordBuffers[audioRecord->index],
                                                     audioRecord->recordBufferSize * sizeof(short));

        // 处理PCM数据(进行编码)
        if (audioRecord->mediaCodec != NULL) {
            audioRecord->mediaCodec->feedInputBufferAndDrainOutputBuffer(
                    audioRecord->mediaCodec->getMediaCodec(),
                    (unsigned char *) audioRecord->recordBuffers[tempIndex],
                    0,
                    tempRecordBufferSize,
                    0,//
                    0,
                    true, false, true);
        }
    } else {
        result = (*audioRecord->recorderRecord)->SetRecordState(
                audioRecord->recorderRecord, SL_RECORDSTATE_STOPPED);
        if (SL_RESULT_SUCCESS == result) {
            //recorderSize = RECORDER_FRAMES * sizeof(short);
        }
        onTransact_release(nullptr, nullptr, 0, nullptr);
    }

    pthread_mutex_unlock(&audioEngineLock);
}