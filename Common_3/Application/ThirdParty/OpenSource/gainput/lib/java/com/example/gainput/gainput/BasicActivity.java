package com.example.gainput.gainput;

import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import java.util.Timer;
import java.util.TimerTask;

import de.johanneskuhlmann.gainput.Gainput;

public class BasicActivity extends Activity
{
    private Gainput gainputHandler;
    private Timer timer;

    public static native void nativeOnCreate();
    public static native void nativeOnUpdate();

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        System.loadLibrary("basicsample");
        gainputHandler = new Gainput(getApplicationContext());
        nativeOnCreate();

        getWindow().getDecorView().findViewById(android.R.id.content).addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) {
                gainputHandler.viewWidth = v.getWidth();
                gainputHandler.viewHeight = v.getHeight();
            }
        });

        timer = new Timer();
        TimerTask t = new TimerTask() {
            int sec = 0;
            @Override
            public void run() {
                nativeOnUpdate();
            }
        };
        timer.scheduleAtFixedRate(t, 500, 33);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event)
    {
        gainputHandler.handleKeyEvent(event);
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event)
    {
        if (gainputHandler.handleTouchEvent(event))
        {
            return true;
        }
        return super.onTouchEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event)
    {
        if (gainputHandler.handleMotionEvent(event))
        {
            return true;
        }
        return super.onGenericMotionEvent(event);
    }

    @Override
    public boolean dispatchKeyEvent (KeyEvent event)
    {
        if (gainputHandler.handleKeyEvent(event))
        {
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
}
