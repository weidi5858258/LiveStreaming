//
// Created by root on 2021/9/3.
//

#include "MediaCodec.h"
#include "AudioRecord.h"
#include "MyJni.h"
#include "include/Log.h"
#include "Send.h"

#define LOG "player_alexander"

extern int release_count;

Send::Send() :
        _isDoing(false),
        _rtmp(nullptr) {
    LOGI("Send::Send()");
    h264_mutex = PTHREAD_MUTEX_INITIALIZER;
    h264_cond = PTHREAD_COND_INITIALIZER;
    aac_mutex = PTHREAD_MUTEX_INITIALIZER;
    aac_cond = PTHREAD_COND_INITIALIZER;
}

Send::~Send() {
    LOGI("Send::~Send()");
    // h264_list aac_list
}

void Send::gameOver() {
    _isDoing = false;

    pthread_mutex_lock(&h264_mutex);
    pthread_cond_signal(&h264_cond);
    pthread_mutex_unlock(&h264_mutex);
    pthread_mutex_lock(&aac_mutex);
    pthread_cond_signal(&aac_cond);
    pthread_mutex_unlock(&aac_mutex);

    pthread_mutex_destroy(&h264_mutex);
    pthread_cond_destroy(&h264_cond);
    pthread_mutex_destroy(&aac_mutex);
    pthread_cond_destroy(&aac_cond);
}

void Send::putH264(RTMPPacket *packet) {
    pthread_mutex_lock(&h264_mutex);
    h264_list.push_back(packet);
    LOGI("Send::putH264()  size: %lld", h264_list.size());
    pthread_cond_signal(&h264_cond);
    pthread_mutex_unlock(&h264_mutex);
}

void Send::putAac(RTMPPacket *packet) {
    pthread_mutex_lock(&aac_mutex);
    aac_list.push_back(packet);
    LOGI("Send::putAac()   size: %lld", aac_list.size());
    pthread_cond_signal(&aac_cond);
    pthread_mutex_unlock(&aac_mutex);
}

void *Send::sendH264(void *arg) {
    if (arg == nullptr) {
        return nullptr;
    }

    Send *send = (Send *) arg;
    RTMPPacket *packet = nullptr;
    //size_t size = 0;
    LOGI("Send::sendH264() start");
    while (send->_isDoing) {
        pthread_mutex_lock(&send->h264_mutex);
        //size = send->h264_list.size();
        if (send->h264_list.size() == 0) {
            //LOGI("Send::sendH264() pthread_cond_wait start");
            pthread_cond_wait(&send->h264_cond, &send->h264_mutex);
            //LOGI("Send::sendH264() pthread_cond_wait end");
            if (!send->_isDoing) {
                pthread_mutex_unlock(&send->h264_mutex);
                break;
            }
        }
        packet = send->h264_list.front();
        send->h264_list.pop_front();
        LOGD("Send::sendH264() size: %lld", send->h264_list.size());
        pthread_mutex_unlock(&send->h264_mutex);

        // send
        if (send->_rtmp) {
            RTMP_SendPacket(send->_rtmp, packet, 1);
        }
        RTMPPacket_Free(packet);
        free(packet);
        packet = nullptr;
    }
    LOGI("Send::sendH264() end");
    release_count++;
    onTransact_release(nullptr, nullptr, 0, nullptr);
}

void *Send::sendAac(void *arg) {
    if (arg == nullptr) {
        return nullptr;
    }

    Send *send = (Send *) arg;
    RTMPPacket *packet = nullptr;
    LOGI("Send::sendAac() start");
    while (send->_isDoing) {
        pthread_mutex_lock(&send->aac_mutex);
        if (send->aac_list.size() == 0) {
            pthread_cond_wait(&send->aac_cond, &send->aac_mutex);
            if (!send->_isDoing) {
                pthread_mutex_unlock(&send->aac_mutex);
                break;
            }
        }
        packet = send->aac_list.front();
        send->aac_list.pop_front();
        LOGD("Send::sendAac()  size: %lld", send->aac_list.size());
        pthread_mutex_unlock(&send->aac_mutex);

        // send
        if (send->_rtmp) {
            RTMP_SendPacket(send->_rtmp, packet, 1);
        }
        RTMPPacket_Free(packet);
        free(packet);
        packet = nullptr;
    }
    LOGI("Send::sendAac() end");
    release_count++;
    onTransact_release(nullptr, nullptr, 0, nullptr);
}

void Send::startSend() {
    LOGI("Send::startSend() start\n");
    _isDoing = true;
    pthread_t p_tids_receive_data;
    pthread_attr_t attr;
    sched_param param;
    pthread_attr_init(&attr);
    pthread_attr_getschedparam(&attr, &param);
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&p_tids_receive_data, &attr, sendH264, this);
    LOGI("Send::startSend() end\n");
}
