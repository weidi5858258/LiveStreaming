package com.weidi.livestreaming;

import android.content.Context;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.util.Log;
import android.view.Surface;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;

public class CameraPreview {

    private static final String TAG = "CameraPreview";

    //    private Camera mCamera;
    private CameraDevice mCameraDevice;
    private String mHostName;
    private Handler mHandler;
    private Context mContext;

    private SurfaceTexture mSurfaceTexture;
    private CaptureRequest.Builder mPreviewBuilder;
    private CameraCaptureSession mPreviewSession;
    private ImageReader imageReader;
    private FrameDataCallBack mFrameDataCallBack;


    private int mPreviewWidth = 720;
    private int mPreviewHeight = 1280;
    private CameraCharacteristics mCharacteristics = null;
    CameraManager mCameraManager = null;
    private boolean cameraOpened = false;
    private byte[] mPreviewData = null;

    public CameraPreview(Handler handler, String name, Context context) {
        mHandler = handler;
        mHostName = name;
        mContext = context;
    }

    public boolean preparePreviewParameters() {
        Log.v(TAG, "prepareParameters start");
        mCameraManager = (CameraManager) mContext.getSystemService(Context.CAMERA_SERVICE);
        if (null != mCameraManager) {
            try {
                mCharacteristics = mCameraManager.getCameraCharacteristics("0");
                if (null == mCharacteristics) {
                    return false;
                } else {
                    return true;
                }
            } catch (CameraAccessException ex) {
                Log.v(TAG, "Unable to get camera ID", ex);
                return false;
            }
        } else {
            Log.v(TAG, "mCameraManager is null");
            return false;
        }
    }

    public boolean initCamera(SurfaceTexture surface) {
        Log.v(TAG, "initCamera start");
        mSurfaceTexture = surface;
        try {
            mCameraManager.openCamera("0", mStateListener, null);
        } catch (SecurityException e) {
            e.printStackTrace();
            return false;
        } catch (CameraAccessException e) {
            e.printStackTrace();
            return false;
        } catch (Exception e) {
            Log.v(TAG, "mCamera open() failed");
            e.printStackTrace();
            return false;
        }
        return true;
    }

    private CameraDevice.StateCallback mStateListener = new CameraDevice.StateCallback() {

        @Override
        public void onOpened(CameraDevice camera) {
            if (camera != null) {
                mCameraDevice = camera;
                cameraOpened = true;
                startPreview();
            } else {
                Log.v(TAG, "error : camera = null");
            }
        }

        @Override
        public void onError(CameraDevice camera, int error) {
            Log.v(TAG, "error : onError be called");
            camera.close();
            mCameraDevice = null;
        }

        @Override
        public void onDisconnected(CameraDevice camera) {
            Log.v(TAG, "onDisconnected be called");
            camera.close();
            mCameraDevice = null;
        }
    };

    private void startPreview() {
        Log.v(TAG, "startPreview start");
        try {
            assert mSurfaceTexture != null;
            mSurfaceTexture.setDefaultBufferSize(mPreviewWidth, mPreviewHeight);

            Surface surface = new Surface(mSurfaceTexture);
            mPreviewBuilder = mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            mPreviewBuilder.set(
                    CaptureRequest.SCALER_CROP_REGION,
                    new Rect(0, 0, mPreviewWidth, mPreviewHeight));
            mPreviewBuilder.set(CaptureRequest.CONTROL_AF_MODE,
                    CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);
            mPreviewBuilder.set(CaptureRequest.CONTROL_AE_MODE,
                    CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH);
            mPreviewBuilder.set(CaptureRequest.CONTROL_MODE, CameraMetadata.CONTROL_MODE_AUTO);

            imageReader = ImageReader.newInstance(mPreviewWidth, mPreviewHeight,
                    ImageFormat.YUV_420_888, 2);
            ImageReader.OnImageAvailableListener mOnImageAvailableListener =
                    new ImageReader.OnImageAvailableListener() {

                        @Override
                        public void onImageAvailable(ImageReader reader) {
                            Image image = null;
                            try {
                                image = reader.acquireNextImage();
                                byte[] data = YUV_420_888_data(image);

                                /*ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
                                for (int i = 0; i < 3; i++) {
                                    ByteBuffer byteBuffer = image.getPlanes()[i].getBuffer();
                                    byte[] bytes = new byte[byteBuffer.remaining()];
                                    byteBuffer.get(bytes);
                                    try {
                                        outputStream.write(bytes);
                                    } catch (IOException e) {
                                        e.printStackTrace();
                                    }
                                }
                                mPreviewData = outputStream.toByteArray();*/
                                if (mFrameDataCallBack != null) {
                                    mFrameDataCallBack.onPreviewFrame(mPreviewData);
                                }
                            } finally {
                                if (image != null) {
                                    image.close();
                                }
                            }
                        }
                    };
            imageReader.setOnImageAvailableListener(mOnImageAvailableListener, mHandler);
            mPreviewBuilder.addTarget(surface);
            mPreviewBuilder.addTarget(imageReader.getSurface());
            try {
                mCameraDevice.createCaptureSession(
                        Arrays.asList(surface, imageReader.getSurface()),
                        new CameraCaptureSession.StateCallback() {
                            @Override
                            public void onConfigured(CameraCaptureSession session) {
                                mPreviewSession = session;
                                try {
                                    mPreviewSession.setRepeatingRequest(
                                            mPreviewBuilder.build(),
                                            null,
                                            null);
                                } catch (CameraAccessException e) {
                                    e.printStackTrace();
                                }
                            }

                            @Override
                            public void onConfigureFailed(CameraCaptureSession session) {
                            }
                        }, null);
            } catch (Exception e) {
                e.printStackTrace();
            }
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }

    }

    public void setFrameDataCallBack(FrameDataCallBack frameDataCallBack) {
        mFrameDataCallBack = frameDataCallBack;
    }


    public interface FrameDataCallBack {
        void onPreviewFrame(byte data[]);
    }

    public void start() {
        if (mCameraDevice != null) {
            Log.v(TAG, mHostName + ", mCamera startPreview()");
            startPreview();
        }
    }

    public void stop() {
        if (mCameraDevice != null) {
            Log.v(TAG, mHostName + ", mCamera stopPreview()");
            try {
                if (mPreviewSession != null) {
                    mPreviewSession.stopRepeating();
                }
            } catch (CameraAccessException e) {
                e.printStackTrace();
            }
        }
    }

    public void release() {
        if (mCameraDevice != null) {
            Log.v(TAG, mHostName + ", mCamera release()");

            if (imageReader != null) {
                imageReader.close();
                imageReader = null;
            }

            if (mPreviewSession != null) {
                mPreviewSession.close();
                mPreviewSession = null;
            }
            if (mCameraDevice != null) {
                mCameraDevice.close();
                mCameraDevice = null;
            }
        }
    }

    public void notifyEndOfTextureDestroyed() {
        release();
    }

    protected CameraDevice getCurrentCamera() {
        return mCameraDevice;
    }

    public CameraCharacteristics getCharacteristics() {
        return mCharacteristics;
    }

    public static byte[] YUV_420_888_data(Image image) {
        final int imageWidth = image.getWidth();
        final int imageHeight = image.getHeight();
        final Image.Plane[] planes = image.getPlanes();
        byte[] data = new byte[imageWidth * imageHeight *
                ImageFormat.getBitsPerPixel(ImageFormat.YUV_420_888) / 8];
        int offset = 0;

        for (int plane = 0; plane < planes.length; ++plane) {
            final ByteBuffer buffer = planes[plane].getBuffer();
            final int rowStride = planes[plane].getRowStride();
            // Experimentally, U and V planes have |pixelStride| = 2, which
            // essentially means they are packed.
            final int pixelStride = planes[plane].getPixelStride();
            final int planeWidth = (plane == 0) ? imageWidth : imageWidth / 2;
            final int planeHeight = (plane == 0) ? imageHeight : imageHeight / 2;
            if (pixelStride == 1 && rowStride == planeWidth) {
                // Copy whole plane from buffer into |data| at once.
                buffer.get(data, offset, planeWidth * planeHeight);
                offset += planeWidth * planeHeight;
            } else {
                // Copy pixels one by one respecting pixelStride and rowStride.
                byte[] rowData = new byte[rowStride];
                for (int row = 0; row < planeHeight - 1; ++row) {
                    buffer.get(rowData, 0, rowStride);
                    for (int col = 0; col < planeWidth; ++col) {
                        data[offset++] = rowData[col * pixelStride];
                    }
                }
                // Last row is special in some devices and may not contain the full
                // |rowStride| bytes of data.
                // See http://developer.android.com/reference/android/media/Image.Plane
                // .html#getBuffer()
                buffer.get(rowData, 0, Math.min(rowStride, buffer.remaining()));
                for (int col = 0; col < planeWidth; ++col) {
                    data[offset++] = rowData[col * pixelStride];
                }
            }
        }

        return data;
    }


}
