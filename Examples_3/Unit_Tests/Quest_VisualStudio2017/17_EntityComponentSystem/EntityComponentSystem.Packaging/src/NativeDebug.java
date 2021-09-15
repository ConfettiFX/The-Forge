
package com.forge.unittest.EntityComponentSystem;

import android.app.NativeActivity;

public class NativeDebug extends NativeActivity 
 {
  static { System.loadLibrary("EntityComponentSystem"); }
}