//
// Created by root on 2021/9/3.
//

#ifndef LIVESTREAMING_SEND_H
#define LIVESTREAMING_SEND_H

#include <pthread.h>
#include <list>
#include "include/rtmp.h"

class Send {
private:
    MediaCodec *videoMediaCodec = nullptr;
    MediaCodec *audioMediaCodec = nullptr;
    AudioRecord *audioRecord = nullptr;

    pthread_mutex_t h264_mutex;
    pthread_cond_t h264_cond;

    pthread_mutex_t aac_mutex;
    pthread_cond_t aac_cond;

    std::list<RTMPPacket *> h264_list;
    std::list<RTMPPacket *> aac_list;
    bool isDoing;
public:
    Send();

    ~Send();

    void setVideoMediaCodec(MediaCodec *codec) {
        videoMediaCodec = codec;
    }

    void setAudioMediaCodec(MediaCodec *codec) {
        audioMediaCodec = codec;
    }

    void setAudioRecord(AudioRecord *record) {
        audioRecord = record;
    }

    void gameOver() {
        isDoing = false;
    }

    void putH264(RTMPPacket *packet);

    void putAac(RTMPPacket *packet);

    void sendH264();

    void sendAac();
};


#endif //LIVESTREAMING_SEND_H
