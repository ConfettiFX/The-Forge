package com.forge.unittest;

import android.app.NativeActivity;
import android.app.Activity;
import android.os.Bundle;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.view.View;

import java.util.concurrent.Semaphore;

public class ForgeBaseActivity extends NativeActivity
{
  private Activity activity;

  static { System.loadLibrary("ForgeGame"); }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    activity = this;
    View decorView = getWindow().getDecorView();
    setImmersiveSticky();
    
    decorView.setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener()
		{
      @Override
      public void onSystemUiVisibilityChange(int visibility)
			{
        setImmersiveSticky();
      }
    });
  }

  protected void onResume()
  {
    super.onResume();
    setImmersiveSticky();
  }

  void setImmersiveSticky()
  {
    View decorView = getWindow().getDecorView();
    decorView.setSystemUiVisibility(
        View.SYSTEM_UI_FLAG_FULLSCREEN
      | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
      | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
      | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
      | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
      | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
    );
  }

  // Use a semaphore to create a modal dialog
  private final Semaphore alertSemaphore = new Semaphore(0, true);
  // This function will be called from C++ by name and signature
  public void showAlert(final String title, final String message)
  {
    this.runOnUiThread(new Runnable()
    {
      public void run()
      {
        AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder
          .setTitle(title)
          .setMessage(message)
          .setPositiveButton("Close", new DialogInterface.OnClickListener()
            {
              public void onClick(DialogInterface dialog, int id)
              {
                alertSemaphore.release();
              }
            })
          .setCancelable(false)
          .create()
          .show();
      }
    });
    try
    {
      alertSemaphore.acquire();
    }
    catch (InterruptedException e) {}
  }
}