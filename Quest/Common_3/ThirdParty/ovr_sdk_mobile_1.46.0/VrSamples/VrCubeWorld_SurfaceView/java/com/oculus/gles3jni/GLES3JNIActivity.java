// Copyright (c) Facebook Technologies, LLC and its affiliates. All Rights reserved.
package com.oculus.sdk.vrcubeworldsv;

import android.app.Activity;
import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class GLES3JNIActivity extends Activity implements SurfaceHolder.Callback {
  // Load the gles3jni library right away to make sure JNI_OnLoad() gets called as the very first
  // thing.
  static {
    System.loadLibrary("vrcubeworldsv");
  }

  private static final String TAG = "VrCubeWorld";

  private SurfaceView mView;
  private SurfaceHolder mSurfaceHolder;
  private long mNativeHandle;

  private void adjustVolume(int direction) {
    AudioManager audio = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
    audio.adjustStreamVolume(AudioManager.STREAM_MUSIC, direction, 0);
  }

  @Override
  protected void onCreate(Bundle icicle) {
    Log.v(TAG, "----------------------------------------------------------------");
    Log.v(TAG, "GLES3JNIActivity::onCreate()");
    super.onCreate(icicle);

    mView = new SurfaceView(this);
    setContentView(mView);
    mView.getHolder().addCallback(this);

    mNativeHandle = GLES3JNILib.onCreate(this);
  }

  @Override
  protected void onStart() {
    Log.v(TAG, "GLES3JNIActivity::onStart()");
    super.onStart();
    GLES3JNILib.onStart(mNativeHandle);
  }

  @Override
  protected void onResume() {
    Log.v(TAG, "GLES3JNIActivity::onResume()");
    super.onResume();
    GLES3JNILib.onResume(mNativeHandle);
  }

  @Override
  protected void onPause() {
    Log.v(TAG, "GLES3JNIActivity::onPause()");
    GLES3JNILib.onPause(mNativeHandle);
    super.onPause();
  }

  @Override
  protected void onStop() {
    Log.v(TAG, "GLES3JNIActivity::onStop()");
    GLES3JNILib.onStop(mNativeHandle);
    super.onStop();
  }

  @Override
  protected void onDestroy() {
    Log.v(TAG, "GLES3JNIActivity::onDestroy()");
    if (mSurfaceHolder != null) {
      GLES3JNILib.onSurfaceDestroyed(mNativeHandle);
    }
    GLES3JNILib.onDestroy(mNativeHandle);
    super.onDestroy();
    mNativeHandle = 0;
  }

  @Override
  public void surfaceCreated(SurfaceHolder holder) {
    Log.v(TAG, "GLES3JNIActivity::surfaceCreated()");
    if (mNativeHandle != 0) {
      GLES3JNILib.onSurfaceCreated(mNativeHandle, holder.getSurface());
      mSurfaceHolder = holder;
    }
  }

  @Override
  public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    Log.v(TAG, "GLES3JNIActivity::surfaceChanged()");
    if (mNativeHandle != 0) {
      GLES3JNILib.onSurfaceChanged(mNativeHandle, holder.getSurface());
      mSurfaceHolder = holder;
    }
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder holder) {
    Log.v(TAG, "GLES3JNIActivity::surfaceDestroyed()");
    if (mNativeHandle != 0) {
      GLES3JNILib.onSurfaceDestroyed(mNativeHandle);
      mSurfaceHolder = null;
    }
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent event) {
    if (mNativeHandle != 0) {
      int keyCode = event.getKeyCode();
      int action = event.getAction();
      if (action != KeyEvent.ACTION_DOWN && action != KeyEvent.ACTION_UP) {
        return super.dispatchKeyEvent(event);
      }
      if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
        adjustVolume(1);
        return true;
      }
      if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
        adjustVolume(-1);
        return true;
      }
      if (action == KeyEvent.ACTION_UP) {
        Log.v(TAG, "GLES3JNIActivity::dispatchKeyEvent( " + keyCode + ", " + action + " )");
      }
      GLES3JNILib.onKeyEvent(mNativeHandle, keyCode, action);
      return true;
    }
    return false;
  }
}
