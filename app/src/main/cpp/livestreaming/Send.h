//
// Created by root on 2021/9/3.
//

#ifndef LIVESTREAMING_SEND_H
#define LIVESTREAMING_SEND_H

#include <pthread.h>
#include <list>

extern "C" {
#include "librtmp/rtmp.h"
};

class Send {
private:
    pthread_mutex_t h264_mutex;
    pthread_cond_t h264_cond;

    pthread_mutex_t aac_mutex;
    pthread_cond_t aac_cond;

    pthread_mutex_t send_mutex;

    std::list<RTMPPacket *> h264_list;
    std::list<RTMPPacket *> aac_list;
    bool _isDoing;
    RTMP *_rtmp;
public:
    Send();

    ~Send();

    void gameOver();

    bool isSending() {
        return _isDoing;
    }

    void setRTMP(RTMP *rtmp) {
        this->_rtmp = rtmp;
    }

    void putH264(RTMPPacket *packet);

    void putAac(RTMPPacket *packet);

    static void *sendH264(void *arg);

    static void *sendAac(void *arg);

    void startSend();
};


#endif //LIVESTREAMING_SEND_H
