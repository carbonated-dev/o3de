/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

package com.amazon.lumberyard.NativeUI;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.widget.TextView;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

public class LumberyardNativeUI
{
    private static final String TAG = "LMBR";
    private static AlertDialog currentDialog;
    private static AtomicReference<String> userSelection;

    public static void DisplayDialog(final Activity activity, final String title, final String message, final String[] options)
    {
        Log.d("LMBR", "DisplayDialog called");

        userSelection = new AtomicReference<String>("");
        userSelection.set("");

        Runnable uiDialog = new Runnable()
        {
            public void run()
            {
                AlertDialog.Builder builder = new AlertDialog.Builder(activity);
                TextView textView = new TextView(activity);
                textView.setText(title + "\n" + message);
                builder.setCustomTitle(textView);
                builder.setItems(options, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int index) {
                        userSelection.set(options[index]);
                        Log.d("LMBR", "Selected option: " + userSelection.get());
                    }
                });
                AlertDialog dialog = builder.create();
                dialog.show();
            }
        };
    
        activity.runOnUiThread(uiDialog);
    }

    public static String GetUserSelection()
    {
        return userSelection.get();
    }

    public static String DisplayDialogCarbonated(final Activity activity, final String title, final String message, final String[] options)
    {
        AtomicReference<String> selection = new AtomicReference<String>("");
        if (!CanShowDialog(activity))
        {
            return selection.get();
        }

        final CountDownLatch latchUserSelection = new CountDownLatch(1);
        final CountDownLatch latchShow = new CountDownLatch(1);

        Runnable uiDialog = () ->
        {
            try
            {
                if (!CanShowDialog(activity))
                {
                    latchShow.countDown();
                    latchUserSelection.countDown();
                    return;
                }

                if (IsDialogShowing())
                {
                    latchShow.countDown();
                    latchUserSelection.countDown();
                    return;
                }

                AlertDialog.Builder builder = new AlertDialog.Builder(activity);
                builder.setTitle(title);
                builder.setMessage(message);
                builder.setItems(options, (dialog, index) ->
                {
                    try
                    {
                        selection.set(options[index]);
                    }
                    catch (Exception e)
                    {
                        Log.e(TAG, "Error in item callback", e);
                        selection.set("");
                    }
                    finally
                    {
                        latchUserSelection.countDown();
                    }
                });

                currentDialog = builder.create();
                currentDialog.setOnShowListener(dialog -> latchShow.countDown());
                currentDialog.setOnDismissListener(dialog -> currentDialog = null);

                currentDialog.show();
            }
            catch (Exception e)
            {
                Log.e(TAG, "Error showing dialog", e);
                latchShow.countDown();
                latchUserSelection.countDown();
            }
        };

        activity.runOnUiThread(uiDialog);

        try
        {
            boolean completed = latchShow.await(5, TimeUnit.SECONDS);
            if (!completed)
            {
                latchUserSelection.countDown();
                return "";
            }
            latchUserSelection.await();
        }
        catch (InterruptedException e)
        {
            Log.e(TAG, "Interrupted while waiting for dialog", e);
            return "";
        }

        return selection.get();
    }

    public static void OnDeadlock(final Activity activity, final String title, final String message)
    {
        Context context = activity.getApplicationContext();
        String dateStr = new SimpleDateFormat("MM-dd-yyyy hh.mma", Locale.US).format(new Date());
        String fileName = "BlockingDialogDeadlock " + dateStr + ".txt";
        File logFile = new File(context.getFilesDir(), fileName);

        try (FileWriter writer = new FileWriter(logFile, true))
        {
            writer.write("Main thread is locked (likely waiting for semaphore) while another thread requests a blocking popup at " + dateStr + ".\n");
            writer.write("Dialog title: " + title + "\n");
            writer.write("Dialog message: " + message + "\n\n");
            writer.flush();
        }
        catch (IOException e)
        {
            Log.e(TAG, "Failed to write deadlock log", e);
        }

        Log.e(TAG, "Deadlock detected at " + dateStr + "\nfile path '" + logFile.getAbsolutePath() + "'");
        Log.e(TAG, "Dialog title: " + title);
        Log.e(TAG, "Dialog message: " + message);

        android.os.Process.killProcess(android.os.Process.myPid());
        System.exit(10);
    }

    public static boolean IsDialogShowing()
    {
        return currentDialog != null && currentDialog.isShowing();
    }

    public static boolean CanShowDialog(final Activity activity)
    {
        if (activity == null)
        {
            return false;
        }

        if (activity.isFinishing() || activity.isDestroyed())
        {
            return false;
        }

        if (activity instanceof LifecycleOwner)
        {
            Lifecycle.State state = ((LifecycleOwner) activity).getLifecycle().getCurrentState();
            if (!state.isAtLeast(Lifecycle.State.RESUMED))
            {
                return false;
            }
        }

        Window window = activity.getWindow();
        if (window == null)
        {
            return false;
        }

        View decorView = window.getDecorView();
        if (!decorView.isShown())
        {
            return false;
        }

        return decorView.hasWindowFocus();
    }
}
