/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
package com.amazon.lumberyard.input;


import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.util.Log;
import android.view.KeyEvent;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.inputmethod.InputMethodManager;
import android.view.View;


////////////////////////////////////////////////////////////////
public class KeyboardHandler
{
    // ----
    // KeyboardHandler (public)
    // ----

    public static native void SendUnicodeText(String unicodeText);


    ////////////////////////////////////////////////////////////////
    public KeyboardHandler(Activity activity)
    {
        m_activity = activity;
        if (m_activity != null)
        {
            m_inputManager = (InputMethodManager)m_activity.getSystemService(Context.INPUT_METHOD_SERVICE);
        }
        else
        {
            m_inputManager = null;
            Log.w(s_tag, "KeyboardHandler ime show failed reason=activity_null will_retry");
        }
    }

    ////////////////////////////////////////////////////////////////
    public void ShowTextInput()
    {
        final Activity activity = m_activity;
        if (activity == null)
        {
            Log.w(s_tag, "KeyboardHandler ime show failed reason=activity_null will_retry");
            return;
        }

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run()
            {
                if (activity.getWindow() == null)
                {
                    Log.w(s_tag, "KeyboardHandler ime show failed reason=window_null will_retry");
                    return;
                }

                View decor = activity.getWindow().getDecorView();
                if (!(decor instanceof ViewGroup))
                {
                    Log.w(s_tag, "KeyboardHandler ime show failed reason=decor_missing will_retry");
                    return;
                }

                ViewGroup viewGroup = (ViewGroup)decor;
                if (m_textView == null)
                {
                    m_textView = new DummyTextView(activity);
                    viewGroup.addView(m_textView);
                }

                int width = decor.getWidth() > 0 ? decor.getWidth() : ViewGroup.LayoutParams.MATCH_PARENT;
                int height = decor.getHeight() > 0 ? decor.getHeight() : ViewGroup.LayoutParams.MATCH_PARENT;
                m_textView.setLayoutParams(new ViewGroup.LayoutParams(width, height));
                m_textView.Show(m_inputManager);
            }
        });
    }

    ////////////////////////////////////////////////////////////////
    public void HideTextInput()
    {
        final Activity activity = m_activity;
        if (activity == null)
        {
            Log.w(s_tag, "KeyboardHandler ime show failed reason=activity_null will_retry");
            return;
        }
        if (m_textView != null)
        {
            final DummyTextView textView = m_textView;
            final InputMethodManager inputManager = m_inputManager;
            activity.runOnUiThread(new Runnable() {
                @Override
                public void run()
                {
                    if (inputManager != null)
                    {
                        inputManager.hideSoftInputFromWindow(textView.getWindowToken(), 0);
                    }
                    else
                    {
                        Log.w(s_tag, "KeyboardHandler ime show failed reason=imm_null will_retry");
                    }
                    textView.Hide();
                }
            });
        }
    }

    ////////////////////////////////////////////////////////////////
    public boolean IsShowing()
    {
        if (m_textView != null)
        {
            return m_textView.IsShowing();
        }
        return false;
    }


    // ----

    private class DummyTextView extends View
    {
        ////////////////////////////////////////////////////////////////
        public DummyTextView(Context context)
        {
            super(context);

            setFocusableInTouchMode(true);
            setFocusable(true);

            m_isShowing = false;
        }

        ////////////////////////////////////////////////////////////////
        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event)
        {
            if (event.isPrintingKey())
            {
                int unicode = event.getUnicodeChar();
                String character = String.valueOf((char)unicode);

                Log.d(s_tag, String.format("OnKeyDown - Unicode: %s - Printed character: %s", unicode, character));
                SendUnicodeText(character);
            }
            return super.onKeyDown(keyCode, event);
        }

        ////////////////////////////////////////////////////////////////
        @Override
        public boolean onKeyMultiple(int keyCode, int count, KeyEvent event)
        {
            if(event.getAction() == KeyEvent.ACTION_MULTIPLE && keyCode == KeyEvent.KEYCODE_UNKNOWN)
            {
                String text = event.getCharacters();
                Log.d(s_tag, String.format("onKeyMultiple - Text: %s", text));
                if (text != null)
                {
                    SendUnicodeText(text);
                }
            }
            return super.onKeyMultiple(keyCode, count, event);
        }

        ////////////////////////////////////////////////////////////////
        @Override
        public boolean onKeyPreIme(int keyCode, KeyEvent event)
        {
            if (keyCode == KeyEvent.KEYCODE_BACK)
            {
                Hide();
            }

            return super.onKeyPreIme(keyCode, event);
        }

        ////////////////////////////////////////////////////////////////
        public boolean IsShowing()
        {
            return m_isShowing;
        }

        ////////////////////////////////////////////////////////////////
        public void Show(InputMethodManager inputManager)
        {
            View decor = GetView();
            if (decor != null)
            {
                m_windowFlags = decor.getSystemUiVisibility();
            }

            setVisibility(View.VISIBLE);
            // Stay false until an inset check sees the IME. A failed show must not block the next tap.
            m_isShowing = false;

            if (!requestFocus())
            {
                Log.w(s_tag, "KeyboardHandler ime show failed reason=focus will_retry");
                return;
            }

            removeCallbacks(m_confirmIme);
            removeCallbacks(m_showTimeout);
            armInsetListener();

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            {
                WindowInsetsController controller = getWindowInsetsController();
                if (controller != null)
                {
                    controller.show(WindowInsets.Type.ime());
                    Log.i(s_tag, "KeyboardHandler ime show requested path=insets result=requested");
                    scheduleImeCheck();
                    return;
                }
                Log.w(s_tag, "KeyboardHandler ime show failed reason=insets_controller_null fallback=imm");
            }

            boolean shown = false;
            if (inputManager != null)
            {
                shown = inputManager.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT);
            }
            Log.i(s_tag, "KeyboardHandler ime show requested path=imm result=" + shown);
            if (!shown)
            {
                Log.w(s_tag, "KeyboardHandler ime show failed reason=" + (inputManager == null ? "imm_null" : "imm_false") + " will_retry");
                m_isShowing = false;
                return;
            }

            scheduleImeCheck();
        }

        ////////////////////////////////////////////////////////////////
        public void Hide()
        {
            removeCallbacks(m_confirmIme);
            removeCallbacks(m_showTimeout);
            setVisibility(View.GONE);
            m_isShowing = false;

            View decor = GetView();
            if (decor != null)
            {
                decor.setSystemUiVisibility(m_windowFlags);
            }
        }


        // ----

        private void armInsetListener()
        {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R)
            {
                return;
            }

            setOnApplyWindowInsetsListener(new OnApplyWindowInsetsListener() {
                @Override
                public WindowInsets onApplyWindowInsets(View view, WindowInsets insets)
                {
                    noteIme(insets);
                    return view.onApplyWindowInsets(insets);
                }
            });
        }

        private void scheduleImeCheck()
        {
            post(m_confirmIme);
            postDelayed(m_confirmIme, 100);
            postDelayed(m_confirmIme, 300);
            postDelayed(m_showTimeout, SHOW_TIMEOUT_MS);
        }

        private boolean noteIme(WindowInsets insets)
        {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R || insets == null)
            {
                return false;
            }
            boolean visible = insets.isVisible(WindowInsets.Type.ime());
            int height = insets.getInsets(WindowInsets.Type.ime()).bottom;
            if (!visible && height <= 0)
            {
                return false;
            }
            if (!m_isShowing)
            {
                m_isShowing = true;
                Log.i(s_tag, "KeyboardHandler ime visible h=" + height);
            }
            removeCallbacks(m_showTimeout);
            return true;
        }

        private final Runnable m_confirmIme = new Runnable() {
            @Override
            public void run()
            {
                if (m_isShowing)
                {
                    return;
                }

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
                {
                    View decor = GetView();
                    WindowInsets insets = decor != null ? decor.getRootWindowInsets() : null;
                    if (insets == null)
                    {
                        insets = getRootWindowInsets();
                    }
                    if (noteIme(insets))
                    {
                        return;
                    }
                }
                else
                {
                    View decor = GetView();
                    if (decor != null)
                    {
                        Rect visibleFrame = new Rect();
                        decor.getWindowVisibleDisplayFrame(visibleFrame);
                        int height = decor.getHeight() - visibleFrame.bottom;
                        if (height > 0)
                        {
                            m_isShowing = true;
                            Log.i(s_tag, "KeyboardHandler ime visible h=" + height);
                            removeCallbacks(m_showTimeout);
                        }
                    }
                }
            }
        };

        private final Runnable m_showTimeout = new Runnable() {
            @Override
            public void run()
            {
                if (!m_isShowing)
                {
                    Log.w(s_tag, "KeyboardHandler ime show failed reason=timeout will_retry");
                    m_isShowing = false;
                }
            }
        };

        private boolean m_isShowing;
        private int m_windowFlags;
    }


    // ----

    ////////////////////////////////////////////////////////////////
    private View GetView()
    {
        if (m_activity == null || m_activity.getWindow() == null)
        {
            return null;
        }
        return m_activity.getWindow().getDecorView();
    }


    // ----

    private static final String s_tag = "KeyboardHandler";
    private static final int SHOW_TIMEOUT_MS = 800;

    private Activity m_activity;
    private InputMethodManager m_inputManager;
    private DummyTextView m_textView;
}
