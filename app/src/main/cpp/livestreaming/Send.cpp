//
// Created by root on 2021/9/3.
//

#include "MediaCodec.h"
#include "AudioRecord.h"
#include "include/Log.h"
#include "include/rtmp.h"
#include "Send.h"

#define LOG "player_alexander"

Send::Send() :
        videoMediaCodec(NULL),
        audioMediaCodec(NULL),
        audioRecord(NULL),
        isDoing(false) {
    LOGI("Send::Send()");
    h264_mutex = PTHREAD_MUTEX_INITIALIZER;
    h264_cond = PTHREAD_COND_INITIALIZER;
    aac_mutex = PTHREAD_MUTEX_INITIALIZER;
    aac_cond = PTHREAD_COND_INITIALIZER;
}

Send::~Send() {
    LOGI("Send::~Send()");
}

void Send::putH264(RTMPPacket *packet) {
    pthread_mutex_lock(&h264_mutex);
    h264_list.push_back(packet);
    pthread_cond_signal(&h264_cond);
    pthread_mutex_unlock(&h264_mutex);
}

void Send::putAac(RTMPPacket *packet) {
    pthread_mutex_lock(&aac_mutex);
    aac_list.push_back(packet);
    pthread_cond_signal(&aac_cond);
    pthread_mutex_unlock(&aac_mutex);
}

void Send::sendH264() {
    RTMPPacket *packet = nullptr;
    size_t size = 0;
    while (isDoing) {
        pthread_mutex_lock(&h264_mutex);
        size = h264_list.size();
        if (size == 0) {
            pthread_cond_wait(&h264_cond, &h264_mutex);
            if (!isDoing) {
                pthread_mutex_unlock(&h264_mutex);
                break;
            }
        }
        packet = h264_list.front();
        h264_list.pop_front();
        pthread_mutex_unlock(&h264_mutex);

        // send
    }
}

void Send::sendAac() {

}
