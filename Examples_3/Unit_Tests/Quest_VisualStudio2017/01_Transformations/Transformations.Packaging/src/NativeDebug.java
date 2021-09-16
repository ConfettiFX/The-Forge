
package com.forge.unittest.Transformations;

import android.app.NativeActivity;

public class NativeDebug extends NativeActivity 
 {
  static { System.loadLibrary("Transformations"); }
}