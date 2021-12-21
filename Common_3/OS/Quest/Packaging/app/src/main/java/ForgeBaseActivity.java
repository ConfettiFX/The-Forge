package com.forge.unittest;

import android.app.NativeActivity;

public class ForgeBaseActivity extends NativeActivity
{
  static { System.loadLibrary("ForgeGame"); }
}