package com.forge.unittest;

import android.app.NativeActivity;
import android.os.Build;
import android.content.Context;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;
import java.util.Set;
import java.util.HashSet;
import androidx.annotation.Keep;

public class ForgeBaseActivity extends NativeActivity
{
    static { System.loadLibrary("ForgeGame"); }

    public static final String LOGTAG = "TheForgeJava";
    private static final int   LOG_LEVEL_RAW = 1;
    private static final int   LOG_LEVEL_DEBUG = 2;
    private static final int   LOG_LEVEL_INFO = 4;
    private static final int   LOG_LEVEL_WARNING = 8;
    private static final int   LOG_LEVEL_ERROR = 16;

    @Keep public float[] getSupportedRefreshRates()
    {
        if (Build.VERSION.SDK_INT < 23)
        {
            nativeLog(LOG_LEVEL_INFO, "Supported Refresh rate API is only avaible in SDK >= 23.");
            return null;
        }

        WindowManager wm = (WindowManager)getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
        Display       display = wm.getDefaultDisplay();
        Display.Mode[] modes = display.getSupportedModes();
        nativeLog(LOG_LEVEL_INFO, "Supported Refresh rates (" + modes.length + ")");

        Set<Float> refreshRatesSet = new HashSet<Float>();
        for (int i = 0; i < modes.length; i += 1)
        {
            nativeLog(LOG_LEVEL_INFO, "  [" + i + "] = " + modes[i].getRefreshRate());
            refreshRatesSet.add(modes[i].getRefreshRate());
        }
        refreshRatesSet.remove(display.getRefreshRate());

        // Unique refresh rates
        int refreshRateIt = 0;
        float[] refreshRates = new float[refreshRatesSet.size() + 1];
        refreshRates[refreshRateIt++] = display.getRefreshRate();
        for (Float x : refreshRatesSet)
        {
            refreshRates[refreshRateIt++] = x;
        }
        return refreshRates;
    }

    private static native void nativeLog(int logLevel, String str);
}