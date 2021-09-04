package com.weidi.livestreaming;

import android.content.Context;
import android.content.Intent;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import static com.weidi.livestreaming.Constants.CREATE_SCREEN_CAPTURE_INTENT;
import static com.weidi.livestreaming.Constants.RELEASE;
import static com.weidi.livestreaming.Constants.SET_ACTIVITY;
import static com.weidi.livestreaming.Constants.SET_MEDIAPROJECTION;
import static com.weidi.livestreaming.Constants.START_SCREEN_RECORD;

public class MainActivity extends AppCompatActivity {

    private static final String TAG =
            "player_alexander";


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        /*getWindow().setFlags(
                WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);*/
        setContentView(R.layout.activity_main);

        internalOnCreate();
    }

    /*********************************
     * Started
     *********************************/

    @Override
    public void onStart() {
        super.onStart();
        Log.i(TAG, "onStart()");
    }

    /*********************************
     * Resumed
     *********************************/

    @Override
    public void onResume() {
        super.onResume();
        Log.i(TAG, "onResume()");

        internalOnResume();
    }

    /*********************************
     * Paused
     *********************************/

    @Override
    public void onPause() {
        super.onPause();
        Log.i(TAG, "onPause()");

        internalOnPause();
    }

    /*********************************
     * Stopped
     *********************************/

    @Override
    public void onStop() {
        super.onStop();
        Log.i(TAG, "onStop()");

        internalOnStop();
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy()");
        internalOnDestroy();

        super.onDestroy();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (data != null) {
            Log.i(TAG, "onActivityResult()" +
                    " requestCode: " + requestCode +
                    " resultCode: " + resultCode +
                    " data: " + data.toString());
        }

        internalonActivityResult(requestCode, resultCode, data);
    }

    ////////////////////////////////////////////////////////////////////////////////////

    private TextView mStartLiveStreamingTV;
    private MediaProjectionManager mMediaProjectionManager;
    private MediaProjection mMediaProjection;

    private void internalOnCreate() {
        EventBusUtils.register(this);

        mMediaProjectionManager =
                (MediaProjectionManager) getSystemService(Context.MEDIA_PROJECTION_SERVICE);
        mStartLiveStreamingTV = findViewById(R.id.start_live_streaming_tv);

        mStartLiveStreamingTV.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //
                EventBusUtils.removeMessages(CREATE_SCREEN_CAPTURE_INTENT);
                EventBusUtils.postDelayed(MainActivity.class.getName(),
                        CREATE_SCREEN_CAPTURE_INTENT, 500, null);
            }
        });

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // 申请浮窗权限
            if (!Settings.canDrawOverlays(this)) {
                Intent intent = new Intent(
                        Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                        Uri.parse("package:" + getPackageName()));
                startActivityForResult(intent, 0);
            } else {
                startService(new Intent(this, MediaClientService.class));
            }
        }
    }

    private void internalOnResume() {

    }

    private void internalOnPause() {
        //mSurfaceViewRoot.removeView(mCameraTextureView);
        //mCameraTextureView.re(mSurfaceHolderCallback);
    }

    private void internalOnStop() {

    }

    private void internalOnDestroy() {
        EventBusUtils.unregister(this);
    }

    private void internalonActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case 0: {
                // onActivityResult() requestCode: 0 resultCode: 0 data: null
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    if (Settings.canDrawOverlays(this)) {
                        Log.i(TAG, "internalonActivityResult() 浮动窗口的权限已申请!!!");
                        startService(new Intent(this, MediaClientService.class));
                    } else {
                        Log.e(TAG, "internalonActivityResult() 浮动窗口的权限已拒绝!!!");
                    }
                }
                break;
            }
            case 1: {
                break;
            }
            case CREATE_SCREEN_CAPTURE_INTENT: {
                // requestCode: 1000 resultCode: -1 data: Intent { (has extras) }
                // MediaProjection对象是这样来的,所以要得到MediaProjection对象,必须同意权限
                if (mMediaProjectionManager != null) {
                    mMediaProjection =
                            mMediaProjectionManager.getMediaProjection(resultCode, data);
                }
                if (mMediaProjection == null) {
                    Log.e(TAG, "internalonActivityResult() mMediaProjection is null");
                    EventBusUtils.postThread(
                            MediaClientService.class.getName(), SET_ACTIVITY, null);
                    EventBusUtils.postThread(
                            MediaClientService.class.getName(), SET_MEDIAPROJECTION, null);
                    EventBusUtils.postThread(
                            MediaClientService.class.getName(), RELEASE, null);
                    return;
                }

                EventBusUtils.postThread(
                        MediaClientService.class.getName(), SET_ACTIVITY,
                        new Object[]{MainActivity.this});
                EventBusUtils.postThread(
                        MediaClientService.class.getName(), SET_MEDIAPROJECTION,
                        new Object[]{mMediaProjection});
                EventBusUtils.postThread(
                        MediaClientService.class.getName(), START_SCREEN_RECORD, null);
                break;
            }
            default:
                break;
        }
    }

    private void requestPermission() {
        Log.i(TAG, "requestPermission()");
        /*Object object = EventBusUtils.post(
                MediaClientService.class, IS_RECORDING, null);
        if (object != null) {
            boolean isRecording = (boolean) object;
            if (isRecording) {
                Log.e(TAG, "requestPermission() return for is recording");
                Toast.makeText(getApplicationContext(), "正在录屏", Toast.LENGTH_SHORT).show();
                return;
            }
        }*/

        if (mMediaProjectionManager != null) {
            startActivityForResult(
                    mMediaProjectionManager.createScreenCaptureIntent(),
                    CREATE_SCREEN_CAPTURE_INTENT);
        }
    }

    private Object onEvent(int what, Object[] objArray) {
        Object result = null;
        switch (what) {
            case 0: {
                break;
            }
            case CREATE_SCREEN_CAPTURE_INTENT: {
                requestPermission();
                break;
            }
            default:
                break;
        }
        return result;
    }

    /*private TextureView.SurfaceTextureListener mSurfaceTextureListener =
            new TextureView.SurfaceTextureListener() {

                @Override
                public void onSurfaceTextureAvailable(
                        @NonNull SurfaceTexture surface, int width, int height) {
                    Log.i(TAG, "onSurfaceTextureAvailable()");
                    if (mCameraPreview.preparePreviewParameters()) {
                        if (mCameraPreview.initCamera(surface)) {
                            mCameraPreview.start();
                        }
                    }
                }

                @Override
                public void onSurfaceTextureSizeChanged(
                        @NonNull SurfaceTexture surface, int width, int height) {

                }

                @Override
                public boolean onSurfaceTextureDestroyed(
                        @NonNull SurfaceTexture surface) {
                    Log.i(TAG, "onSurfaceTextureDestroyed()");
                    mCameraPreview.stop();
                    mCameraPreview.release();
                    return false;
                }

                @Override
                public void onSurfaceTextureUpdated(
                        @NonNull SurfaceTexture surface) {

                }
            };*/


}