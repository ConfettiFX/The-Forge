// Copyright (c) Facebook Technologies, LLC and its affiliates. All Rights reserved.
package com.oculus.sdk.vrcubeworldsv;

import android.app.Activity;
import android.view.Surface;

// Wrapper for native library

public class GLES3JNILib {
  // Activity lifecycle
  public static native long onCreate(Activity obj);

  public static native void onStart(long handle);

  public static native void onResume(long handle);

  public static native void onPause(long handle);

  public static native void onStop(long handle);

  public static native void onDestroy(long handle);

  // Surface lifecycle
  public static native void onSurfaceCreated(long handle, Surface s);

  public static native void onSurfaceChanged(long handle, Surface s);

  public static native void onSurfaceDestroyed(long handle);

  // Input
  public static native void onKeyEvent(long handle, int keyCode, int action);
}
