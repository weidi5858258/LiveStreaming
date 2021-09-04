//
// Created by root on 2021/9/3.
//

#ifndef LIVESTREAMING_SEND_H
#define LIVESTREAMING_SEND_H

#include <pthread.h>
#include <list>
extern "C" {
#include "include/rtmp/rtmp.h"
};

class Send {
private:
    pthread_mutex_t h264_mutex;
    pthread_cond_t h264_cond;

    pthread_mutex_t aac_mutex;
    pthread_cond_t aac_cond;

    std::list<RTMPPacket *> h264_list;
    std::list<RTMPPacket *> aac_list;
    bool isDoing;
    RTMP* _rtmp;
public:
    Send();

    ~Send();

    void gameOver() {
        isDoing = false;
    }

    void setRTMP(RTMP *rtmp) {
        this->_rtmp = rtmp;
    }

    void putH264(RTMPPacket *packet);

    void putAac(RTMPPacket *packet);

    void sendH264();

    void sendAac();
};


#endif //LIVESTREAMING_SEND_H
