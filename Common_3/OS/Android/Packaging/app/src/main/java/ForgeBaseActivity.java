package com.forge.unittest;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.util.Log;
import android.view.View;
import java.util.HashMap;
import java.util.concurrent.Semaphore;

public class ForgeBaseActivity extends NativeActivity
{
    private Activity activity;

    private HashMap<Integer, Integer> mMobileThermalMap;
    private static final int          MOBILE_THERMAL_NOT_SUPPORTED = -2;
    private static final int          MOBILE_THERMAL_ERROR = -1;

    // Library name to be loaded is defined thorugh configuration
    public static final String META_DATA_LIB_NAME = "android.app.lib_name";

    @Override protected void onCreate(Bundle savedInstanceState)
    {
        String libname = "ForgeGame";

        // https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/app/NativeActivity.java
        try
        {
            ActivityInfo activityInfo = getPackageManager().getActivityInfo(getIntent().getComponent(), PackageManager.GET_META_DATA);
            if (activityInfo.metaData != null)
            {
                String ln = activityInfo.metaData.getString(META_DATA_LIB_NAME);
                if (ln != null)
                    libname = ln;
            }
        }
        catch (PackageManager.NameNotFoundException e)
        {
            throw new RuntimeException("Error getting activity info", e);
        }

        System.loadLibrary(libname);

        super.onCreate(savedInstanceState);

        activity = this;
        View decorView = getWindow().getDecorView();
        setImmersiveSticky();

        decorView.setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener() {
            @Override public void onSystemUiVisibilityChange(int visibility) { setImmersiveSticky(); }
        });

        mMobileThermalMap = new HashMap<>();
        try
        {
            if (Build.VERSION.SDK_INT >= 29)
            {
                mMobileThermalMap.put(PowerManager.THERMAL_STATUS_NONE, 0);      // ThermalStatus::THERMAL_STATUS_NONE
                mMobileThermalMap.put(PowerManager.THERMAL_STATUS_LIGHT, 1);     // ThermalStatus::THERMAL_STATUS_LIGHT
                mMobileThermalMap.put(PowerManager.THERMAL_STATUS_MODERATE, 2);  // ThermalStatus::THERMAL_STATUS_MODERATE
                mMobileThermalMap.put(PowerManager.THERMAL_STATUS_SEVERE, 3);    // ThermalStatus::THERMAL_STATUS_SEVERE
                mMobileThermalMap.put(PowerManager.THERMAL_STATUS_CRITICAL, 4);  // ThermalStatus::THERMAL_STATUS_CRITICAL
                mMobileThermalMap.put(PowerManager.THERMAL_STATUS_EMERGENCY, 5); // ThermalStatus::THERMAL_STATUS_EMERGENCY
                mMobileThermalMap.put(PowerManager.THERMAL_STATUS_SHUTDOWN, 6);  // ThermalStatus::THERMAL_STATUS_SHUTDOWN
            }
        }
        catch (Exception e)
        {
            Log.i("TheForge", "Failed to populate mMobileThermalMap: " + e.getMessage());
        }

        try
        {
            if (Build.VERSION.SDK_INT >= 29)
            {
                PowerManager pManager = (PowerManager)getApplicationContext().getSystemService(Context.POWER_SERVICE);
                pManager.addThermalStatusListener((int status) -> {
                    Integer nativeStatus = mMobileThermalMap.get(status);
                    nativeThermalEvent((nativeStatus != null) ? nativeStatus : MOBILE_THERMAL_ERROR);
                });
            }
            else
            {
                nativeThermalEvent(MOBILE_THERMAL_NOT_SUPPORTED);
            }
        }
        catch (Exception e)
        {
            Log.i("TheForge", "Failed to setup thermal status listener: " + e.getMessage());
        }
    }

    protected void onResume()
    {
        super.onResume();
        setImmersiveSticky();
    }

    void setImmersiveSticky()
    {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                                        View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                                        View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    private static native void nativeOnAlertClosed();

    // This function will be called from C++ by name and signature
    public void showAlert(final String title, final String message)
    {
        this.runOnUiThread(new Runnable() {
            public void run()
            {
                AlertDialog.Builder builder = new AlertDialog.Builder(activity);
                builder.setTitle(title)
                    .setMessage(message)
                    .setPositiveButton("Close",
                                       new DialogInterface.OnClickListener() {
                                           public void onClick(DialogInterface dialog, int id) { nativeOnAlertClosed(); }
                                       })
                    .setCancelable(false)
                    .show();
            }
        });
    }

    // Native thermal event entry point defined in AndroidBase.cpp
    private static native void nativeThermalEvent(int nativeStatus);
}