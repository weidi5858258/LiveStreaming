package com.weidi.livestreaming;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.BitmapFactory;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.AudioRecord;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.IBinder;
import android.provider.MediaStore;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.OrientationEventListener;
import android.view.Surface;
import android.view.View;
import android.view.WindowManager;

import java.nio.ByteBuffer;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import static com.weidi.livestreaming.Constants.ACCELEROMETER_ROTATION;
import static com.weidi.livestreaming.Constants.IS_RECORDING;
import static com.weidi.livestreaming.Constants.MAINACTIVITY_ON_RESUME;
import static com.weidi.livestreaming.Constants.RELEASE;
import static com.weidi.livestreaming.Constants.SET_ACTIVITY;
import static com.weidi.livestreaming.Constants.SET_MEDIAPROJECTION;
import static com.weidi.livestreaming.Constants.START_SCREEN_RECORD;
import static com.weidi.livestreaming.Constants.STOP_SCREEN_RECORD;
import static com.weidi.livestreaming.MyJni.DO_SOMETHING_CODE_find_createLandscapeVirtualDisplay;
import static com.weidi.livestreaming.MyJni.DO_SOMETHING_CODE_find_createPortraitVirtualDisplay;
import static com.weidi.livestreaming.MyJni.DO_SOMETHING_CODE_find_encoder_send_data_error;
import static com.weidi.livestreaming.MyJni.DO_SOMETHING_CODE_is_recording;
import static com.weidi.livestreaming.MyJni.DO_SOMETHING_CODE_start_audio_record;
import static com.weidi.livestreaming.MyJni.DO_SOMETHING_CODE_start_screen_record;
import static com.weidi.livestreaming.MyJni.DO_SOMETHING_CODE_stop_screen_record;
import static com.weidi.livestreaming.MyJni.ENCODER_MEDIA_CODEC_GO_JNI;

public class MediaClientService extends Service {

    public MediaClientService() {
    }

    @Override
    public IBinder onBind(Intent intent) {
        // TODO: Return the communication channel to the service.
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public boolean onUnbind(Intent intent) {
        return super.onUnbind(intent);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        internalOnCreate();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        internalOnStartCommand(intent, flags, startId);
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        internalOnDestroy();
        super.onDestroy();
    }

    private static final String TAG = "player_alexander";
    private WindowManager mWindowManager;
    private DisplayMetrics mDisplayMetrics;
    private View mView;

    private void internalOnCreate() {
        Log.i(TAG, "MediaClientService internalOnCreate()");
        EventBusUtils.register(this);
        mContext = getApplicationContext();
        mMyJni = MyJni.getDefault();
        // 为了进程保活
        mWindowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        LayoutInflater inflater =
                (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mView = inflater.inflate(R.layout.transparent_layout, null);
        WindowManager.LayoutParams layoutParams = new WindowManager.LayoutParams();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            layoutParams.type = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
        } else {
            layoutParams.type = WindowManager.LayoutParams.TYPE_PHONE;
        }
        // 创建非模态,不可碰触
        layoutParams.flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
        layoutParams.gravity = Gravity.TOP + Gravity.START;
        layoutParams.width = 1;
        layoutParams.height = 1;
        layoutParams.x = 0;
        layoutParams.y = 0;
        mWindowManager.addView(mView, layoutParams);

        mMediaProjectionManager =
                (MediaProjectionManager) getSystemService(Context.MEDIA_PROJECTION_SERVICE);

        myOrientationListener = new OrientationListener(this);
        boolean autoRotateOn = Settings.System.getInt(getContentResolver(),
                Settings.System.ACCELEROMETER_ROTATION, 0) == 1;
        // 检查系统是否开启自动旋转
        if (autoRotateOn && !enable) {
            Log.i(TAG, "myOrientationListener.enable()");
            enable = true;
            myOrientationListener.enable();
        }

        int2Bytes(ORIENTATION_PORTRAIT, 1);
        int2Bytes(ORIENTATION_LANDSCAPE, 1);
        ORIENTATION_PORTRAIT[4] = -1;// 服务端读到"-1"表示需要竖屏
        ORIENTATION_LANDSCAPE[4] = -2;// 服务端读到"-2"表示需要横屏
    }

    private void internalOnStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) {
            return;
        }
        String action = intent.getStringExtra("action");
        if (TextUtils.equals(action, "GetMediaProjection")) {
            createNotificationChannel();
            int resultCode = intent.getIntExtra("code", -1);
            Intent data = intent.getParcelableExtra("data");
            mMediaProjection = mMediaProjectionManager.getMediaProjection(resultCode, data);
            EventBusUtils.postThread(
                    MediaClientService.class.getName(), START_SCREEN_RECORD, null);
        }
    }

    private void internalOnDestroy() {
        Log.i(TAG, "MediaClientService internalOnDestroy()");
        releaseAll();
        sps_pps_portrait = null;
        sps_pps_landscape = null;
        if (mWindowManager != null && mView != null) {
            mWindowManager.removeView(mView);
            mWindowManager = null;
            mView = null;
        }
        if (myOrientationListener != null && enable) {
            enable = false;
            myOrientationListener.disable();
        }
        if (ENCODER_MEDIA_CODEC_GO_JNI) {
            // notify to jni for free sps_pps
            //mMyJni.onTransact(DO_SOMETHING_CODE_release_sps_pps, null);
        }
        EventBusUtils.unregister(this);
    }

    ////////////////////////////////////////////////////////////////////////////

    public static final String FLAG = "@@@@@";

    private Activity mActivity;
    private Context mContext;
    private MyJni mMyJni;

    private Object lock = new Object();
    private boolean mIsRecording = false;
    private boolean mIsConnected = false;
    private boolean allowSendOrientation = false;
    private boolean mIsHandlingPortrait = false;
    private boolean mIsHandlingLandscape = false;
    private final Lock lockPortrait = new ReentrantLock();
    private final Condition conditionPortrait = lockPortrait.newCondition();
    private final Lock lockLandscape = new ReentrantLock();
    private final Condition conditionLandscape = lockLandscape.newCondition();
    private MediaProjectionManager mMediaProjectionManager;
    private MediaProjection mMediaProjection;
    private VirtualDisplay mVirtualDisplay;
    private int whatIsDevice = 1;

    private int mScreenWidthPortrait;
    private int mScreenHeightPortrait;
    private int mScreenWidthLandscape;
    private int mScreenHeightLandscape;
    private Surface mSurfacePortrait;
    private Surface mSurfaceLandscape;
    private String mVideoEncoderCodecName;
    private String mAudioEncoderCodecName;
    private String mVideoMime;
    private String mAudioMime;
    private MediaCodec mVideoEncoderMediaCodecPortrait;
    private MediaCodec mVideoEncoderMediaCodecLandscape;
    private MediaFormat mVideoEncoderMediaFormatPortrait;
    private MediaFormat mVideoEncoderMediaFormatLandscape;
    private OrientationListener myOrientationListener;
    private boolean enable = false;
    private int mPreOrientation;
    private int mCurOrientation;
    private byte[] ORIENTATION_PORTRAIT = new byte[5];
    private byte[] ORIENTATION_LANDSCAPE = new byte[5];
    private boolean mIsGettingSpsPps = true;
    private byte[] sps_pps_portrait = null;
    private byte[] sps_pps_landscape = null;

    private boolean mIsKeyFrameWritePortrait = false;
    private boolean mIsKeyFrameWriteLandscape = false;

    private MediaCodec mediaCodec;
    private AudioRecord audioRecord;
    private byte[] audioData;
    private int sizeInBytes;
    private JniObject audioJniObject = JniObject.obtain();

    private Object onEvent(int what, Object[] objArray) {
        Object result = null;
        switch (what) {
            case START_SCREEN_RECORD: {
                startScreenRecordForNative();
                break;
            }
            case STOP_SCREEN_RECORD: {
                stopScreenRecordForNative();
                break;
            }
            case RELEASE: {
                releaseAll();
                break;
            }
            case SET_ACTIVITY: {
                if (objArray != null && objArray.length > 0) {
                    mActivity = (Activity) objArray[0];
                } else {
                    mActivity = null;
                }
                break;
            }
            case SET_MEDIAPROJECTION: {
                if (objArray != null && objArray.length > 0) {
                    mMediaProjection = (MediaProjection) objArray[0];
                } else {
                    if (mMediaProjection != null) {
                        mMediaProjection.stop();
                        mMediaProjection.unregisterCallback(mMediaProjectionCallback);
                    }
                    mMediaProjection = null;
                }
                break;
            }
            case IS_RECORDING: {
                if (ENCODER_MEDIA_CODEC_GO_JNI) {
                    String str = mMyJni.onTransact(DO_SOMETHING_CODE_is_recording, null);
                    if (TextUtils.isEmpty(str)) {
                        return false;
                    }
                    return Boolean.parseBoolean(str);
                }
                return mIsRecording;
            }
            /*case SET_IP_AND_PORT: {
                if (objArray != null && objArray.length > 1) {
                    IP = (String) objArray[0];
                    PORT = (int) objArray[1];
                }
                break;
            }
            case START_RECORD_SCREEN: {
                mThreadHandler.removeMessages(START_RECORD_SCREEN);
                mThreadHandler.sendEmptyMessageDelayed(START_RECORD_SCREEN, 500);
                break;
            }
            case STOP_RECORD_SCREEN: {
                mThreadHandler.removeMessages(STOP_RECORD_SCREEN);
                mThreadHandler.sendEmptyMessageDelayed(STOP_RECORD_SCREEN, 500);
                break;
            }
            case RELEASE: {
                mThreadHandler.removeMessages(RELEASE);
                mThreadHandler.sendEmptyMessageDelayed(RELEASE, 500);
                break;
            }*/
            case ACCELEROMETER_ROTATION: {
                boolean autoRotateOn = Settings.System.getInt(getContentResolver(),
                        Settings.System.ACCELEROMETER_ROTATION, 0) == 1;
                // 检查系统是否开启自动旋转
                if (autoRotateOn && !enable) {
                    Log.i(TAG, "myOrientationListener.enable()");
                    enable = true;
                    myOrientationListener.enable();
                }
                break;
            }
            case DO_SOMETHING_CODE_find_createPortraitVirtualDisplay: {
                if (mActivity != null) {
                    mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
                }
                createPortraitVirtualDisplay();
                break;
            }
            case DO_SOMETHING_CODE_find_createLandscapeVirtualDisplay: {
                if (mActivity != null) {
                    mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
                }
                createLandscapeVirtualDisplay();
                break;
            }
            case DO_SOMETHING_CODE_find_encoder_send_data_error: {
                releaseAll();
                EventBusUtils.post(MainActivity.class.getName(), MAINACTIVITY_ON_RESUME, null);
                break;
            }
            default:
                break;
        }
        return result;
    }

    private synchronized void releaseAll() {
        Log.i(TAG, "releaseAll start");
        mActivity = null;
        mIsRecording = false;
        mIsConnected = false;
        allowSendOrientation = false;
        mIsHandlingPortrait = false;
        mIsHandlingLandscape = false;
        mIsKeyFrameWritePortrait = false;
        mIsKeyFrameWriteLandscape = false;

        if (mMediaProjection != null) {
            mMediaProjection.stop();
            mMediaProjection.unregisterCallback(mMediaProjectionCallback);
            mMediaProjection = null;
        }
        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
        MediaUtils.releaseMediaCodec(mVideoEncoderMediaCodecPortrait);
        MediaUtils.releaseMediaCodec(mVideoEncoderMediaCodecLandscape);
        mVideoEncoderMediaCodecPortrait = null;
        mVideoEncoderMediaCodecLandscape = null;
        mVideoEncoderMediaFormatPortrait = null;
        mVideoEncoderMediaFormatLandscape = null;
        mSurfacePortrait = null;
        mSurfaceLandscape = null;
        mVideoEncoderCodecName = null;
        mVideoMime = null;
        Log.i(TAG, "releaseAll end");
    }

    private synchronized boolean startScreenRecordForNative() {
        if (mActivity == null) {
            Log.e(TAG, "startScreenRecordForNative() return for activity is null");
            return false;
        }
        if (mMediaProjection == null) {
            Log.e(TAG, "startScreenRecordForNative() return for mMediaProjection is null");
            return false;
        }

        Log.i(TAG, "startScreenRecordForNative() start");

        String str = mMyJni.onTransact(
                MyJni.DO_SOMETHING_CODE_init_rtmp, null);
        if (TextUtils.isEmpty(str)) {
            releaseAll();
            return false;
        }
        if (Integer.parseInt(str) < 0) {
            Log.e(TAG, "startScreenRecordForNative() DO_SOMETHING_CODE_init_rtmp failed");
            releaseAll();
            return false;
        }

        if (TextUtils.isEmpty(mVideoEncoderCodecName)) {
            mVideoEncoderCodecName = MyJni.getEncoderCodecName(MediaFormat.MIMETYPE_VIDEO_AVC);
            mVideoMime = MediaFormat.MIMETYPE_VIDEO_AVC;
        }
        if (TextUtils.isEmpty(mVideoEncoderCodecName)) {
            Log.e(TAG, "startScreenRecordForNative() mVideoEncoderCodecName is null");
            releaseAll();
            return false;
        }
        Log.i(TAG,
                "startScreenRecordForNative() mVideoEncoderCodecName: " + mVideoEncoderCodecName);

        mPreOrientation = mCurOrientation = getResources().getConfiguration().orientation;
        mDisplayMetrics = new DisplayMetrics();
        mWindowManager.getDefaultDisplay().getRealMetrics(mDisplayMetrics);

        int width = 0;
        int height = 0;
        int tempOrientation = mCurOrientation;
        if (tempOrientation == Configuration.ORIENTATION_PORTRAIT) {
            mScreenWidthPortrait = mDisplayMetrics.widthPixels;
            mScreenHeightPortrait = mDisplayMetrics.heightPixels;
            mScreenWidthLandscape = mScreenHeightPortrait;
            mScreenHeightLandscape = mScreenWidthPortrait;
            width = mScreenWidthPortrait;
            height = mScreenHeightPortrait;
        } else if (tempOrientation == Configuration.ORIENTATION_LANDSCAPE) {
            mScreenWidthLandscape = mDisplayMetrics.widthPixels;
            mScreenHeightLandscape = mDisplayMetrics.heightPixels;
            mScreenWidthPortrait = mScreenHeightLandscape;
            mScreenHeightPortrait = mScreenWidthLandscape;
            width = mScreenWidthLandscape;
            height = mScreenHeightLandscape;
        }

        String deviceName = Settings.Global.getString(
                getContentResolver(), Settings.Global.DEVICE_NAME);
        if (TextUtils.isEmpty(deviceName)) {
            deviceName = "LiveStreaming";
        }
        // 先向服务端发送配置信息,服务端拿到这些信息可以先初始化一些东西
        StringBuilder sb = new StringBuilder();
        sb.append(deviceName);// 设备名称
        sb.append(FLAG);
        sb.append(mVideoMime);// mime
        sb.append(FLAG);
        if (tempOrientation == Configuration.ORIENTATION_PORTRAIT) {
            sb.append(mScreenWidthPortrait);
            sb.append(FLAG);
            sb.append(mScreenHeightPortrait);
        } else if (tempOrientation == Configuration.ORIENTATION_LANDSCAPE) {
            sb.append(mScreenWidthLandscape);
            sb.append(FLAG);
            sb.append(mScreenHeightLandscape);
        }
        sb.append(FLAG);
        sb.append(tempOrientation);// 横屏还是竖屏

        JniObject jniObject = JniObject.obtain();
        jniObject.valueIntArray = new int[]{
                sb.length(), tempOrientation, width, height};
        jniObject.valueStringArray = new String[]{
                sb.toString(), mVideoMime, mVideoEncoderCodecName};
        str = mMyJni.onTransact(
                MyJni.DO_SOMETHING_CODE_start_screen_record_prepare, jniObject);
        if (TextUtils.isEmpty(str)) {
            releaseAll();
            return false;
        }
        boolean ret = Boolean.parseBoolean(str);
        if (!ret) {
            releaseAll();
            return false;
        }

        mSurfacePortrait = (Surface) jniObject.valueObjectArray[0];
        mSurfaceLandscape = (Surface) jniObject.valueObjectArray[1];
        jniObject = null;

        jniObject = JniObject.obtain();
        jniObject.valueIntArray = new int[]{
                MediaUtils.sampleRateInHz,
                MediaUtils.channelCount,
                MediaUtils.channelConfig,
                MediaUtils.getMinBufferSize() * 2};
        mAudioMime = MediaFormat.MIMETYPE_AUDIO_AAC;
        mAudioEncoderCodecName = MyJni.getEncoderCodecName(mAudioMime);
        Log.i(TAG,
                "startScreenRecordForNative() mAudioEncoderCodecName: " + mAudioEncoderCodecName);
        jniObject.valueStringArray = new String[]{mAudioMime, mAudioEncoderCodecName};
        mMyJni.onTransact(
                MyJni.DO_SOMETHING_CODE_start_audio_record_prepare, jniObject);

        mMediaProjection.registerCallback(mMediaProjectionCallback,
                EventBusUtils.getThreadHandler());

        mMyJni.onTransact(DO_SOMETHING_CODE_start_screen_record, null);
        mMyJni.onTransact(DO_SOMETHING_CODE_start_audio_record, null);
        EventBusUtils.postUi(MainActivity.class.getName(), MAINACTIVITY_ON_RESUME, null);

        // Test
        /*mediaCodec = MediaUtils.getAudioEncoderMediaCodec();
        audioRecord = MediaUtils.createAudioRecord();
        audioRecord.startRecording();
        sizeInBytes = MediaUtils.getMinBufferSize() * 2;
        audioData = new byte[sizeInBytes];
        new Thread(new Runnable() {
            @Override
            public void run() {
                for (; ; ) {
                    int size = audioRecord.read(audioData, 0, sizeInBytes);
                    //Log.i(TAG, "startScreenRecordForNative() size: " + size);
                    audioJniObject.valueByteArray = audioData;
                    audioJniObject.valueInt = size;
                    audioJniObject.valueLong = System.currentTimeMillis() * 1000;
                    mMyJni.onTransact(DO_SOMETHING_CODE_start_audio_record, audioJniObject);

                    EDMediaCodec.feedInputBufferAndDrainOutputBuffer(
                            mCallback,
                            EDMediaCodec.TYPE.TYPE_AUDIO,
                            mediaCodec,
                            audioData,
                            0,
                            size,
                            System.currentTimeMillis() * 1000,
                            0,
                            true,
                            true);
                }
            }
        }).start();*/

        Log.i(TAG, "startScreenRecordForNative() end");
        return true;
    }

    private synchronized void stopScreenRecordForNative() {
        Log.i(TAG, "stopScreenRecordForNative() start");
        mMyJni.onTransact(DO_SOMETHING_CODE_stop_screen_record, null);
        releaseAll();
        EventBusUtils.postUi(MainActivity.class.getName(), MAINACTIVITY_ON_RESUME, null);
        Log.i(TAG, "stopScreenRecordForNative() end");
    }

    /***
     竖屏有竖屏的Surface,横屏有横屏的Surface.
     因此需要有竖屏的MediaCodec和横屏的MediaCodec.
     */
    private void createPortraitVirtualDisplay() {
        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
        mVirtualDisplay = mMediaProjection.createVirtualDisplay(
                "createPortraitVirtualDisplay",
                mScreenWidthPortrait,
                mScreenHeightPortrait,
                mDisplayMetrics.densityDpi,
                DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC,
                mSurfacePortrait,
                null,
                null);
        Log.d(TAG, "createPortraitVirtualDisplay() " +
                "  mScreenWidthPortrait: " + mScreenWidthPortrait +
                "  mScreenHeightPortrait: " + mScreenHeightPortrait);
    }

    private void createLandscapeVirtualDisplay() {
        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
        mVirtualDisplay = mMediaProjection.createVirtualDisplay(
                //TAG + "-Display",
                "createLandscapeVirtualDisplay",
                mScreenWidthLandscape,
                mScreenHeightLandscape,
                mDisplayMetrics.densityDpi,
                DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC,
                mSurfaceLandscape,
                null,
                null);
        Log.d(TAG, "createLandscapeVirtualDisplay()" +
                " mScreenWidthLandscape: " + mScreenWidthLandscape +
                " mScreenHeightLandscape: " + mScreenHeightLandscape);
    }

    private void fromPortraitToLandscape() {
        Log.w(TAG, "竖屏 ---> 横屏");
        createLandscapeVirtualDisplay();
        mIsHandlingLandscape = true;
        mIsHandlingPortrait = false;
        mIsKeyFrameWriteLandscape = false;
        //sendData(ORIENTATION_LANDSCAPE);
        lockLandscape.lock();
        conditionLandscape.signal();
        lockLandscape.unlock();
    }

    private void fromLandscapeToPortrait() {
        Log.w(TAG, "横屏 ---> 竖屏");
        createPortraitVirtualDisplay();
        mIsHandlingPortrait = true;
        mIsHandlingLandscape = false;
        mIsKeyFrameWritePortrait = false;
        //sendData(ORIENTATION_PORTRAIT);
        lockPortrait.lock();
        conditionPortrait.signal();
        lockPortrait.unlock();
    }

    private MediaProjection.Callback mMediaProjectionCallback =
            new MediaProjection.Callback() {
                @Override
                public void onStop() {
                    Log.i(TAG, "MediaProjection.Callback onStop()");
                }
            };

    /***
     * 将int转为长度为4的byte数组
     *
     * @param length
     * @return
     */
    private static void int2Bytes(byte[] frame, int length) {
        frame[0] = (byte) length;
        frame[1] = (byte) (length >> 8);
        frame[2] = (byte) (length >> 16);
        frame[3] = (byte) (length >> 24);
    }

    private void createNotificationChannel() {
        //获取一个Notification构造器
        Notification.Builder builder = new Notification.Builder(this.getApplicationContext());
        //点击后跳转的界面，可以设置跳转数据
        Intent intent = new Intent(this, MainActivity.class);

        // 设置PendingIntent
        builder.setContentIntent(PendingIntent.getActivity(this, 0, intent, 0))
                // 设置下拉列表中的图标(大图标)
                .setLargeIcon(BitmapFactory.decodeResource(this.getResources(), R.mipmap.ic_launcher))
                .setContentTitle("A")// 设置下拉列表里的标题
                .setSmallIcon(R.mipmap.ic_launcher)// 设置状态栏内的小图标
                .setContentText("B")// 设置上下文内容
                .setWhen(System.currentTimeMillis());// 设置该通知发生的时间

        /*以下是对Android 8.0的适配*/
        // 普通notification适配
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            builder.setChannelId("notification_id");
            // 前台服务notification适配
            NotificationManager notificationManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            NotificationChannel channel = new NotificationChannel("notification_id", "notification_name", NotificationManager.IMPORTANCE_LOW);
            notificationManager.createNotificationChannel(channel);
        }

        // 获取构建好的Notification
        Notification notification = builder.build();
        // 设置为默认的声音
        notification.defaults = Notification.DEFAULT_SOUND;
        startForeground(110, notification);
    }

    private class OrientationListener extends OrientationEventListener {

        public OrientationListener(Context context) {
            super(context);
        }

        public OrientationListener(Context context, int rate) {
            super(context, rate);
        }

        @Override
        public void onOrientationChanged(int orientation) {
            //Log.d(TAG, "orention" + orientation);
            if (!allowSendOrientation) {
                return;
            }
            if (((orientation >= 0) && (orientation < 45)) || (orientation > 315)) {
                //Log.d(TAG, "设置竖屏");
                mCurOrientation = Configuration.ORIENTATION_PORTRAIT;
            } else if (orientation > 225 && orientation < 315) {
                //Log.d(TAG, "设置横屏");
                mCurOrientation = Configuration.ORIENTATION_LANDSCAPE;
            } else if (orientation > 45 && orientation < 135) {
                //Log.d(TAG, "反向横屏");
                mCurOrientation = Configuration.ORIENTATION_LANDSCAPE;
            } else if (orientation > 135 && orientation < 225) {
                //Log.d(TAG, "反向竖屏");
                mCurOrientation = Configuration.ORIENTATION_PORTRAIT;
            }

            if (mPreOrientation != mCurOrientation) {
                if (mPreOrientation == Configuration.ORIENTATION_PORTRAIT) {
                    // 竖屏 ---> 横屏
                    if (ENCODER_MEDIA_CODEC_GO_JNI) {
                        //mMyJni.onTransact(DO_SOMETHING_CODE_fromPortraitToLandscape, null);
                    } else {
                        fromPortraitToLandscape();
                    }
                } else {
                    // 横屏 ---> 竖屏
                    if (ENCODER_MEDIA_CODEC_GO_JNI) {
                        //mMyJni.onTransact(DO_SOMETHING_CODE_fromLandscapeToPortrait, null);
                    } else {
                        fromLandscapeToPortrait();
                    }
                }
                mPreOrientation = mCurOrientation;
            }
        }
    }

    private EDMediaCodec.Callback mCallback = new EDMediaCodec.Callback() {
        @Override
        public boolean isVideoFinished() {
            return false;
        }

        @Override
        public boolean isAudioFinished() {
            return false;
        }

        @Override
        public void handleVideoOutputFormat(MediaFormat mediaFormat) {

        }

        @Override
        public void handleAudioOutputFormat(MediaFormat mediaFormat) {

        }

        @Override
        public int handleVideoOutputBuffer(ByteBuffer room, MediaCodec.BufferInfo roomInfo) {
            return 0;
        }

        @Override
        public int handleAudioOutputBuffer(ByteBuffer room, MediaCodec.BufferInfo roomInfo) {
            return 0;
        }
    };

}