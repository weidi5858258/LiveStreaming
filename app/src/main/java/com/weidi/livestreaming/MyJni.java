package com.weidi.livestreaming;

import android.content.Context;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.text.TextUtils;
import android.util.Log;

import java.util.Iterator;
import java.util.concurrent.ArrayBlockingQueue;

/***
 http://anddymao.com/2019/10/16/2019-10-16-ndk-MediaCodec/
 NDK中使用MediaCodec编解码视频
 */
public class MyJni {

    private static final String TAG = "player_alexander";

    static {
        try {
            System.loadLibrary("livestreaming");
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "卧槽, livestreaming库加载失败了!!!");
            error.printStackTrace();
        }
    }

    private volatile static MyJni sMyJni;

    private MyJni() {
    }

    public static MyJni getDefault() {
        if (sMyJni == null) {
            synchronized (MyJni.class) {
                if (sMyJni == null) {
                    sMyJni = new MyJni();
                }
            }
        }
        return sMyJni;
    }

    // java ---> jni
    public static final int DO_SOMETHING_CODE_init = 1000;
    public static final int DO_SOMETHING_CODE_init_rtmp = 1001;
    public static final int DO_SOMETHING_CODE_start_screen_record_prepare = 1012;
    public static final int DO_SOMETHING_CODE_start_screen_record = 1013;
    public static final int DO_SOMETHING_CODE_stop_screen_record = 1014;
    public static final int DO_SOMETHING_CODE_start_audio_record_prepare = 1015;
    public static final int DO_SOMETHING_CODE_start_audio_record = 1016;
    public static final int DO_SOMETHING_CODE_stop_audio_record = 1017;
    public static final int DO_SOMETHING_CODE_is_recording = 1018;

    // jni ---> java
    public static final int DO_SOMETHING_CODE_connected = 2000;
    public static final int DO_SOMETHING_CODE_disconnected = 2001;
    public static final int DO_SOMETHING_CODE_change_window = 2002;
    public static final int DO_SOMETHING_CODE_find_decoder_codec_name = 2003;
    public static final int DO_SOMETHING_CODE_find_createPortraitVirtualDisplay = 2004;
    public static final int DO_SOMETHING_CODE_find_createLandscapeVirtualDisplay = 2005;
    public static final int DO_SOMETHING_CODE_find_encoder_send_data_error = 2006;

    public static final boolean ENCODER_MEDIA_CODEC_GO_JNI = true;
    public static final boolean DECODER_MEDIA_CODEC_GO_JNI = true;

    private Context mContext;

    public native String onTransact(int code, JniObject jniObject);

    public void setContext(Context context) {
        mContext = context;
    }

    public void onDestroy() {
    }

    private void jni2Java(int code, JniObject jniObject) {
        switch (code) {
            case DO_SOMETHING_CODE_connected: {
                break;
            }
            case DO_SOMETHING_CODE_find_createPortraitVirtualDisplay: {
                EventBusUtils.post(MediaClientService.class.getName(),
                        DO_SOMETHING_CODE_find_createPortraitVirtualDisplay, null);
                break;
            }
            default:
                break;
        }
    }

    private void drainFrame(ArrayBlockingQueue<byte[]> queue, long takeCount) {
        int size1 = queue.size();
        if (/*size1 >= 5 && */takeCount >= 10) {
            boolean firstKeyFrame = false;
            int count = 0;
            for (byte[] frame : queue) {
                ++count;
                firstKeyFrame = ((frame[frame.length - 1] & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0);
                if (firstKeyFrame) {
                    break;
                }
            }
            if (firstKeyFrame && count > 1) {
                //Log.e(TAG, "drainFrame() size 1: " + size1);
                Iterator<byte[]> iterator = queue.iterator();
                while (iterator.hasNext()) {
                    byte[] frame = iterator.next();
                    if (((frame[frame.length - 1] & MediaCodec.BUFFER_FLAG_KEY_FRAME) == 0)) {
                        iterator.remove();
                    } else {
                        break;
                    }
                }
                int size2 = queue.size();
                //Log.i(TAG, "drainFrame() size 2: " + size2);
                if (/*size2 >= 5 && */size1 > size2) {
                    boolean secondKeyFrame = false;
                    for (byte[] frame : queue) {
                        firstKeyFrame =
                                ((frame[frame.length - 1] & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0);
                        if (firstKeyFrame && secondKeyFrame) {
                            break;
                        } else if (firstKeyFrame) {
                            secondKeyFrame = true;
                        }
                    }
                    if (firstKeyFrame && secondKeyFrame) {
                        secondKeyFrame = false;
                        iterator = queue.iterator();
                        while (iterator.hasNext()) {
                            byte[] frame = iterator.next();
                            if (((frame[frame.length - 1] & MediaCodec.BUFFER_FLAG_KEY_FRAME) == 0)) {
                                iterator.remove();
                            } else {
                                if (secondKeyFrame) {
                                    break;
                                }
                                secondKeyFrame = true;
                            }
                        }
                        //size1 = queue.size();
                        //Log.d(TAG, "drainFrame() size 3: " + size1);
                    }
                }
            }
        }
    }

    public static String getDecoderCodecName(String mime) {
        String codecName = null;
        // 查找解码器名称
        MediaCodecInfo[] mediaCodecInfos = MediaUtils.findAllDecodersByMime(mime);
        for (MediaCodecInfo mediaCodecInfo : mediaCodecInfos) {
            if (mediaCodecInfo == null) {
                continue;
            }
            codecName = mediaCodecInfo.getName();
            if (TextUtils.isEmpty(codecName)) {
                continue;
            }
            String tempCodecName = codecName.toLowerCase();
            if (tempCodecName.startsWith("omx.google.")
                    || tempCodecName.startsWith("c2.android.")
                    || tempCodecName.endsWith(".secure")// 用于加密的视频
                    || (!tempCodecName.startsWith("omx.") && !tempCodecName.startsWith("c2."))) {
                codecName = null;
                continue;
            }
            break;

            // 软解
            /*if (!tempCodecName.endsWith(".secure")
                    && (tempCodecName.startsWith("omx.google.")
                    || tempCodecName.startsWith("c2.android."))) {
                break;
            }*/
        }

        return codecName;
    }

    // 根据mime查找编码器
    public static String getEncoderCodecName(String mime) {
        String codecName = null;
        MediaCodecInfo[] mediaCodecInfos =
                MediaUtils.findAllEncodersByMime(mime);
        for (MediaCodecInfo mediaCodecInfo : mediaCodecInfos) {
            if (mediaCodecInfo == null) {
                continue;
            }
            codecName = mediaCodecInfo.getName();
            if (TextUtils.isEmpty(codecName)) {
                continue;
            }
            String tempCodecName = codecName.toLowerCase();
            // 硬解
            // OMX.qcom.video.encoder.avc
            // OMX.google.aac.encoder
            if (mime.startsWith("video/")) {
                if (tempCodecName.startsWith("omx.google.")
                        || tempCodecName.startsWith("c2.android.")
                        // 用于加密的视频
                        || tempCodecName.endsWith(".secure")
                        || (!tempCodecName.startsWith("omx.") && !tempCodecName.startsWith("c2."))) {
                    codecName = null;
                    continue;
                }
            }
            break;

            // 软解
            /*if (!tempCodecName.endsWith(".secure")
                    && (tempCodecName.startsWith("omx.google.")
                    || tempCodecName.startsWith("c2.android."))) {
                break;
            }*/
        }

        return codecName;
    }

}
