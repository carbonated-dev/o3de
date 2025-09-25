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
    private static final int TIMEOUT = 3;
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
        final CountDownLatch latchUserSelection = new CountDownLatch(1);
        final CountDownLatch latchShow = new CountDownLatch(1);

        if (!IsActivityResumed(activity))
        {
            Log.e(TAG, "The dialog cannot be shown because the application is in the background.");
            return "";
        }

        if (IsDialogShowing())
        {
            Log.e(TAG, "The dialog cannot be shown because another window is already shown.");
            return "";
        }

        Runnable uiDialog = () ->
        {
            try
            {
                AlertDialog.Builder builder = new AlertDialog.Builder(activity);
                TextView textView = new TextView(activity);
                textView.setText(title + "\n" + message);
                builder.setCustomTitle(textView);
                builder.setItems(options, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int index) {
                        try
                        {
                            selection.set(options[index]);
                            Log.d(TAG, "Selected option: " + selection.get());
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
                    }
                });

                currentDialog = builder.create();
                currentDialog.setOnShowListener(dialog -> latchShow.countDown());
                currentDialog.setOnDismissListener(dialog -> {
                    currentDialog = null;
                    latchUserSelection.countDown();
                });

                currentDialog.setCancelable(false);
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
            boolean completed = latchShow.await(TIMEOUT, TimeUnit.SECONDS);
            if (completed && currentDialog != null && currentDialog.isShowing() && IsActivityResumed(activity))
            {
                latchUserSelection.await();
            }
            else
            {
                Log.e(TAG, "Can't show dialog");
                return "";
            }
        }
        catch (Exception e)
        {
            Log.e(TAG, "Interrupted while waiting for dialog", e);
            return "";
        }

        return selection.get();
    }

    private static boolean IsActivityResumed(Activity activity)
    {
        if (activity == null)
        {
            return false;
        }

        if (activity instanceof LifecycleOwner)
        {
            LifecycleOwner lifecycleOwner = (LifecycleOwner) activity;
            return lifecycleOwner.getLifecycle().getCurrentState().isAtLeast(Lifecycle.State.RESUMED);
        }

        try
        {
            return activity.hasWindowFocus() && !activity.isFinishing() && !activity.isDestroyed();
        }
        catch (Exception e)
        {
            return false;
        }
    }

    public static void OnDeadlock(final Activity activity, final String title, final String message)
    {
        Context context = activity.getApplicationContext();
        String dateStr = new SimpleDateFormat("MM-dd-yyyy hh.mma", Locale.US).format(new Date());
        String fileName = "BlockingDialogDeadlock " + dateStr + ".txt";

        File logDir = context.getExternalFilesDir("deadlock_logs");
        if (logDir == null) {
            Log.e(TAG, "External files dir is null, fallback to internal");
            logDir = new File(context.getFilesDir(), "deadlock_logs");
        }

        File logFile = new File(logDir, fileName);

        try (FileWriter writer = new FileWriter(logFile, true))
        {
            writer.write("At the moment, there is no way to show a native dialog (the application is in the background or for some other reason)");
            writer.write("Date: " + dateStr + "\n");
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
    }

    public static boolean IsDialogShowing()
    {
        return currentDialog != null && currentDialog.isShowing();
    }
}
