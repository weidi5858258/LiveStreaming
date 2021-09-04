//
// Created by root on 19-8-13.
//

#ifndef USEFRAGMENTS_FFMPEG_H
#define USEFRAGMENTS_FFMPEG_H

enum {
    USE_MODE_MEDIA = 1,
    USE_MODE_ONLY_VIDEO = 2,
    USE_MODE_ONLY_AUDIO = 3,
    USE_MODE_AUDIO_VIDEO = 4,
    USE_MODE_AAC_H264 = 5,
    USE_MODE_MEDIA_4K = 6,
    USE_MODE_MEDIA_MEDIACODEC = 7,
    USE_MODE_MEDIA_FFPLAY = 8
};

enum DO_SOMETHING_CODE {
    // java ---> jni
    DO_SOMETHING_CODE_init = 1000,
    DO_SOMETHING_CODE_init_rtmp = 1001,
    DO_SOMETHING_CODE_start_screen_record_prepare = 1012,
    DO_SOMETHING_CODE_start_screen_record = 1013,
    DO_SOMETHING_CODE_stop_screen_record = 1014,
    DO_SOMETHING_CODE_start_audio_record_prepare = 1015,
    DO_SOMETHING_CODE_start_audio_record = 1016,
    DO_SOMETHING_CODE_stop_audio_record = 1017,
    DO_SOMETHING_CODE_is_recording = 1018,
    DO_SOMETHING_CODE_fromPortraitToLandscape = 1016,
    DO_SOMETHING_CODE_fromLandscapeToPortrait = 1017,
    DO_SOMETHING_CODE_release_sps_pps = 1018,
    DO_SOMETHING_CODE_only_output_key_frame = 1019,

    // jni ---> java
    DO_SOMETHING_CODE_connected = 2000,
    DO_SOMETHING_CODE_disconnected = 2001,
    DO_SOMETHING_CODE_change_window = 2002,
    DO_SOMETHING_CODE_find_decoder_codec_name = 2003,
    DO_SOMETHING_CODE_find_createPortraitVirtualDisplay = 2004,
    DO_SOMETHING_CODE_find_createLandscapeVirtualDisplay = 2005,
    DO_SOMETHING_CODE_find_encoder_send_data_error = 2006,

};

enum {
    MSG_ON_TRANSACT_VIDEO_PRODUCER = 0x1001,
    MSG_ON_TRANSACT_VIDEO_CONSUMER = 0x1002,
    MSG_ON_TRANSACT_AUDIO_PRODUCER = 0x1003,
    MSG_ON_TRANSACT_AUDIO_CONSUMER = 0x1004,
    MSG_ON_TRANSACT_INIT_VIDEO_MEDIACODEC = 0x1005,
};

char *findDecoderCodecName(int code, int which_client, const char *data, ssize_t size);

void createPortraitVirtualDisplay();

void createLandscapeVirtualDisplay();

jint onTransact_release(JNIEnv *env, jobject myJniObject, jint code, jobject jniObject);

#endif //USEFRAGMENTS_FFMPEG_H
