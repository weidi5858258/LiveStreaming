//
// Created by ex-wangliwei on 2021/6/28.
//

#include <jni.h>
#include <string>
#include <sys/system_properties.h>
#include "include/Log.h"
#include "MyJni.h"
#include "MediaCodec.h"
#include "AudioRecord.h"
#include "Send.h"

#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket)+RTMP_MAX_HEADER_SIZE)

// 这个是自定义的LOG的标识
#define LOG "player_alexander"


/***
 https://www.jianshu.com/p/259a31f628a4
 Android Studio+LLDB调试内核Binder

 https://www.jb51.net/article/183149.htm
 创建Android守护进程实例(底层服务)
 */

// 这个值在任何情况下都不要置为"NULL"
static JavaVM *gJavaVm = nullptr;

// JniObject的Class
jclass jniObject_jclass = nullptr;
jfieldID valueObject_jfieldID = nullptr;
jfieldID valueString_jfieldID = nullptr;
jfieldID valueInt_jfieldID = nullptr;
jfieldID valueLong_jfieldID = nullptr;
jfieldID valueByte_jfieldID = nullptr;
jfieldID valueBoolean_jfieldID = nullptr;
jfieldID valueFloat_jfieldID = nullptr;
jfieldID valueDouble_jfieldID = nullptr;
// array
jfieldID valueObjectArray_jfieldID = nullptr;
jfieldID valueStringArray_jfieldID = nullptr;
jfieldID valueIntArray_jfieldID = nullptr;
jfieldID valueLongArray_jfieldID = nullptr;
jfieldID valueByteArray_jfieldID = nullptr;
jfieldID valueBooleanArray_jfieldID = nullptr;
jfieldID valueFloatArray_jfieldID = nullptr;
jfieldID valueDoubleArray_jfieldID = nullptr;

// 下面的jobject,jmethodID按照java的反射过程去理解,套路(jni层调用java层方法)跟反射是一样的
// java层MyJni对象
jobject myJniJavaObject = nullptr;
jmethodID jni2JavaMethodID = nullptr;

static MediaCodec *screenRecordMediaCodec = nullptr;
static MediaCodec *audioRecordMediaCodec = nullptr;
static AudioRecord *audioRecord = nullptr;
static Send *send = nullptr;
static RTMP *rtmp = nullptr;

int release_count = 0;

/***
 called at the library loaded.
 这个方法只有放在这个文件里才有效,在其他文件不会被回调
 */
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD("JNI_OnLoad()\n");
    gJavaVm = vm;
    /*int ret = av_jni_set_java_vm(vm, nullptr);
    if (ret < 0) {
        LOGE("JNI_OnLoad() av_jni_set_java_vm() error ret: %d\n", ret);
    }*/
    return JNI_VERSION_1_6;
}

// 这个方法只有放在这个文件里才有效,在其他文件调用失效
bool getEnv(JNIEnv **env) {
    jint jResult = gJavaVm->GetEnv((void **) env, JNI_VERSION_1_6);
    if (jResult != JNI_OK) {
        if (jResult == JNI_EDETACHED) {
            if (gJavaVm->AttachCurrentThread(env, NULL) != JNI_OK) {
                LOGE("getEnv() AttachCurrentThread Failed.\n");
                *env = nullptr;
                return false;
            }
            return true;
        } else {
            LOGE("getEnv() Failed.\n");
            *env = nullptr;
            return false;
        }
    }

    return false;
}

void jni2Java(JNIEnv *jniEnv, int code, jobject jniObject) {
    if (jniEnv != nullptr
        && myJniJavaObject != nullptr
        && jni2JavaMethodID != nullptr) {
        jniEnv->CallVoidMethod(myJniJavaObject, jni2JavaMethodID, code, jniObject);
    }
}

void sendDataError() {
    JNIEnv *jniEnv;
    bool isAttached = getEnv(&jniEnv);
    jniEnv->CallVoidMethod(myJniJavaObject, jni2JavaMethodID,
                           DO_SOMETHING_CODE_find_encoder_send_data_error, nullptr);
    if (isAttached) {
        gJavaVm->DetachCurrentThread();
    }
}

int getSdkVersion() {
    char sdk[128] = "0";

    // 获取版本号方法
    __system_property_get("ro.build.version.sdk", sdk);
    //__system_property_read_callback();

    //将版本号转为 int 值
    int sdk_verison = atoi(sdk);

    return sdk_verison;
}

void closeJni() {
    JNIEnv *env;
    bool isAttached = getEnv(&env);
    if (myJniJavaObject) {
        env->DeleteGlobalRef(myJniJavaObject);
        myJniJavaObject = nullptr;
    }
    if (jniObject_jclass) {
        env->DeleteGlobalRef(jniObject_jclass);
        jniObject_jclass = nullptr;
    }
    if (isAttached) {
        gJavaVm->DetachCurrentThread();
    }
}

void createPortraitVirtualDisplay() {
    JNIEnv *jniEnv;
    bool isAttached = getEnv(&jniEnv);
    jniEnv->CallVoidMethod(myJniJavaObject, jni2JavaMethodID,
                           DO_SOMETHING_CODE_find_createPortraitVirtualDisplay, nullptr);
    if (isAttached) {
        gJavaVm->DetachCurrentThread();
    }
}

/////////////////////////////////////////////////////////////////////////

static jint onTransact_stop(JNIEnv *env, jobject myJniObject, jint code, jobject jniObject) {
    send->gameOver();
    audioRecord->gameOver();
    screenRecordMediaCodec->gameOver();
    audioRecordMediaCodec->gameOver();
    return JNI_OK;
}

jint onTransact_release(JNIEnv *env, jobject myJniObject, jint code, jobject jniObject) {
    LOGI("onTransact_release() start");
    if (release_count < 2) {
        return JNI_ERR;
    }
    if (rtmp) {
        if (RTMP_IsConnected(rtmp)) {
            RTMP_Close(rtmp);
        }
        RTMP_Free(rtmp);
        rtmp = nullptr;
    }
    if (screenRecordMediaCodec) {
        delete screenRecordMediaCodec;
        screenRecordMediaCodec = nullptr;
    }
    if (audioRecordMediaCodec) {
        delete audioRecordMediaCodec;
        audioRecordMediaCodec = nullptr;
    }
    if (audioRecord) {
        delete audioRecord;
        audioRecord = nullptr;
    }
    if (send) {
        delete send;
        send = nullptr;
    }
    release_count = 0;
    LOGI("onTransact_release() end");
    return JNI_OK;
}

static jint onTransact_init(JNIEnv *env, jobject myJniObject, jint code, jobject jniObject) {
    if (jniObject_jclass != nullptr) {
        env->DeleteGlobalRef(jniObject_jclass);
        jniObject_jclass = nullptr;
    }
    jclass tempJniObjectClass = env->FindClass("com/weidi/livestreaming/JniObject");
    jniObject_jclass = reinterpret_cast<jclass>(env->NewGlobalRef(tempJniObjectClass));
    env->DeleteLocalRef(tempJniObjectClass);

    valueObject_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueObject", "Ljava/lang/Object;");
    valueString_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueString", "Ljava/lang/String;");
    valueInt_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueInt", "I");
    valueLong_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueLong", "J");
    valueByte_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueByte", "B");
    valueBoolean_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueBoolean", "Z");
    valueFloat_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueFloat", "F");
    valueDouble_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueDouble", "D");

    valueObjectArray_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueObjectArray", "[Ljava/lang/Object;");
    valueStringArray_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueStringArray", "[Ljava/lang/String;");
    valueIntArray_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueIntArray", "[I");
    valueLongArray_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueLongArray", "[J");
    valueByteArray_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueByteArray", "[B");
    valueBooleanArray_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueBooleanArray", "[Z");
    valueFloatArray_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueFloatArray", "[F");
    valueDoubleArray_jfieldID = env->GetFieldID(
            jniObject_jclass, "valueDoubleArray", "[D");

    /////////////////////////////////////////////////////////////////////////////////

    if (myJniJavaObject != nullptr) {
        env->DeleteGlobalRef(myJniJavaObject);
        myJniJavaObject = nullptr;
    }
    myJniJavaObject = reinterpret_cast<jobject>(env->NewGlobalRef(myJniObject));
    jclass MyJniClass = env->GetObjectClass(myJniObject);

    // 第三个参数: 括号中是java端方法的参数签名,括号后面是java端方法的返回值签名(V表示void)
    jni2JavaMethodID = env->GetMethodID(
            MyJniClass, "jni2Java", "(ILcom/weidi/livestreaming/JniObject;)V");

    env->DeleteLocalRef(MyJniClass);
    MyJniClass = nullptr;

    return (jint) 0;
}

static jint onTransact_init_rtmp(JNIEnv *env, jobject myJniObject, jint code, jobject jniObject) {
    do {
        rtmp = RTMP_Alloc();
        RTMP_Init(rtmp);
        rtmp->Link.timeout = 10;
        if (!RTMP_SetupURL(rtmp, "rtmp://192.168.43.182/live/stream")) {
            LOGE("onTransact_init_rtmp() RTMP_SetupURL");
            onTransact_release(env, myJniObject, code, jniObject);
            return JNI_ERR;
        }
        RTMP_EnableWrite(rtmp);
        if (!RTMP_Connect(rtmp, NULL)) {
            LOGE("onTransact_init_rtmp() RTMP_Connect");
            onTransact_release(env, myJniObject, code, jniObject);
            return JNI_ERR;
        }
        if (!RTMP_ConnectStream(rtmp, 0)) {
            LOGE("onTransact_init_rtmp() RTMP_ConnectStream");
            onTransact_release(env, myJniObject, code, jniObject);
            return JNI_ERR;
        }
    } while (0);

    LOGI("onTransact_init_rtmp() success");
    screenRecordMediaCodec = new MediaCodec();
    audioRecordMediaCodec = new MediaCodec();
    audioRecord = new AudioRecord();
    send = new Send();

    send->setRTMP(rtmp);
    screenRecordMediaCodec->setRTMP(rtmp);
    screenRecordMediaCodec->setSend(send);
    audioRecordMediaCodec->setRTMP(rtmp);
    audioRecordMediaCodec->setSend(send);
    audioRecord->setMediaCodec(audioRecordMediaCodec);

    return JNI_OK;
}

static jint onTransact_closeJni(JNIEnv *env, jobject thiz,
                                jint code, jobject jniObject) {
    closeJni();
}

/////////////////////////////////////////////////////////////////////////

char *getStrFromDO_SOMETHING_CODE(DO_SOMETHING_CODE code) {
    char info[100];
    memset(info, '\0', sizeof(info));
    switch (code) {
        case DO_SOMETHING_CODE_init:
            //return "DO_SOMETHING_CODE_init";
            strncpy(info, "DO_SOMETHING_CODE_init",
                    strlen("DO_SOMETHING_CODE_init"));
            break;
        default:
            //return "DO_SOMETHING_CODE_nothing";
            strncpy(info, "DO_SOMETHING_CODE_nothing",
                    strlen("DO_SOMETHING_CODE_nothing"));
            break;
    }
    return info;
}

/***
 * jobject thiz 代表定义native方法的类的对象(如果native方法不是static的话)
 */
extern "C"
JNIEXPORT jstring
Java_com_weidi_livestreaming_MyJni_onTransact(JNIEnv *env, jobject thiz,
                                              jint code, jobject jniObject) {
    /*LOGI("onTransact() %s\n",
         getStrFromDO_SOMETHING_CODE(static_cast<DO_SOMETHING_CODE>(code)));*/
    const char ret[] = "0";
    switch (code) {
        case DO_SOMETHING_CODE_init: {
            return env->NewStringUTF(
                    std::to_string(onTransact_init(env, thiz, code, jniObject)).c_str());
        }
        case DO_SOMETHING_CODE_init_rtmp: {
            return env->NewStringUTF(
                    std::to_string(onTransact_init_rtmp(env, thiz, code, jniObject)).c_str());
        }
        case DO_SOMETHING_CODE_start_screen_record_prepare: {
            jobject intArrayObject = env->GetObjectField(jniObject, valueIntArray_jfieldID);
            jobject stringArrayObject = env->GetObjectField(jniObject, valueStringArray_jfieldID);
            if (intArrayObject != nullptr && stringArrayObject != nullptr) {
                // int[]
                jint *intArray = reinterpret_cast<jint *>(
                        env->GetIntArrayElements(static_cast<jintArray>(intArrayObject), nullptr));
                int info_length = intArray[0];
                int orientation = intArray[1];
                int width = intArray[2];
                int height = intArray[3];
                // String[]
                jobjectArray stringArray = reinterpret_cast<jobjectArray>(stringArrayObject);
                jstring info_ = static_cast<jstring>(env->GetObjectArrayElement(stringArray, 0));
                jstring mime_ = static_cast<jstring>(env->GetObjectArrayElement(stringArray, 1));
                jstring codec_name_ =
                        static_cast<jstring>(env->GetObjectArrayElement(stringArray, 2));
                const char *info = env->GetStringUTFChars(info_, 0);
                const char *mime = env->GetStringUTFChars(mime_, 0);
                const char *codec_name = env->GetStringUTFChars(codec_name_, 0);

                screenRecordMediaCodec->startScreenRecordEncoderMediaCodec(
                        mime, codec_name, width, height);
                ANativeWindow *surface = screenRecordMediaCodec->getScreenRecordSurface();

                env->ReleaseStringUTFChars(info_, info);
                env->ReleaseStringUTFChars(mime_, mime);
                env->ReleaseStringUTFChars(codec_name_, codec_name);
                env->DeleteLocalRef(intArrayObject);
                env->DeleteLocalRef(stringArrayObject);

                // 设置Surface
                jobject surface1 = ANativeWindow_toSurface(env, surface);
                jobject surface2 = ANativeWindow_toSurface(env, surface);
                jclass elementClass = env->FindClass("java/lang/Object");
                jobjectArray objectArray = env->NewObjectArray(2, elementClass, nullptr);
                env->SetObjectArrayElement(objectArray, 0, surface1);
                env->SetObjectArrayElement(objectArray, 1, surface2);
                env->SetObjectField(jniObject, valueObjectArray_jfieldID, objectArray);
                // objectArray不确定要不要被释放
                env->DeleteLocalRef(elementClass);
                elementClass = nullptr;
                return env->NewStringUTF("true");
            }
            return env->NewStringUTF("false");
        }
        case DO_SOMETHING_CODE_start_screen_record: {
            screenRecordMediaCodec->startScreenRecord();
            send->startSend();
            return env->NewStringUTF(ret);
        }
        case DO_SOMETHING_CODE_stop_screen_record: {
            onTransact_stop(env, thiz, code, jniObject);
            return env->NewStringUTF(ret);
        }
        case DO_SOMETHING_CODE_start_audio_record_prepare: {
            return env->NewStringUTF(ret);
        }
        case DO_SOMETHING_CODE_start_audio_record: {
            audioRecord->startRecording();
            return env->NewStringUTF(ret);
        }
        case DO_SOMETHING_CODE_stop_audio_record: {
            return env->NewStringUTF(ret);
        }
        case DO_SOMETHING_CODE_is_recording: {
            if (send) {
                return env->NewStringUTF(send->isSending() ? "true" : "false");
            }
            return env->NewStringUTF("false");
        }
        default:
            break;
    }

    return env->NewStringUTF("-1");
}
