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
                    // Mark GONE before hideSoftInput so the inset that follows is ignored.
                    textView.Hide();
                    if (inputManager != null)
                    {
                        inputManager.hideSoftInputFromWindow(textView.getWindowToken(), 0);
                    }
                    else
                    {
                        Log.w(s_tag, "KeyboardHandler ime show failed reason=imm_null will_retry");
                    }
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
            m_showPending = false;
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
            return m_isShowing || m_showPending;
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
            // Visible only after the ime inset. Pending covers the gap so TextEntryStop can still hide.
            m_isShowing = false;

            if (!requestFocus())
            {
                m_showPending = false;
                Log.w(s_tag, "KeyboardHandler ime show failed reason=focus will_retry");
                return;
            }

            removeCallbacks(m_confirmIme);
            removeCallbacks(m_showTimeout);
            armInsetListener();

            WindowInsetsController controller = getWindowInsetsController();
            if (controller != null)
            {
                controller.show(WindowInsets.Type.ime());
                m_showPending = true;
                Log.i(s_tag, "KeyboardHandler ime show requested path=insets result=requested");
                scheduleImeCheck();
                return;
            }

            Log.w(s_tag, "KeyboardHandler ime show failed reason=insets_controller_null fallback=imm");
            boolean shown = false;
            if (inputManager != null)
            {
                shown = inputManager.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT);
            }
            Log.i(s_tag, "KeyboardHandler ime show requested path=imm result=" + shown);
            if (!shown)
            {
                m_showPending = false;
                m_isShowing = false;
                Log.w(s_tag, "KeyboardHandler ime show failed reason=" + (inputManager == null ? "imm_null" : "imm_false") + " will_retry");
                return;
            }

            m_showPending = true;
            scheduleImeCheck();
        }

        ////////////////////////////////////////////////////////////////
        public void Hide()
        {
            removeCallbacks(m_confirmIme);
            removeCallbacks(m_showTimeout);
            setVisibility(View.GONE);
            m_isShowing = false;
            m_showPending = false;

            View decor = GetView();
            if (decor != null)
            {
                decor.setSystemUiVisibility(m_windowFlags);
            }
        }


        // ----

        private void armInsetListener()
        {
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
            if (insets == null || getVisibility() != View.VISIBLE)
            {
                return false;
            }

            boolean visible = insets.isVisible(WindowInsets.Type.ime());
            int height = insets.getInsets(WindowInsets.Type.ime()).bottom;
            if (!visible)
            {
                // Not-yet-visible while pending is the wait, not a dismiss. Only a confirmed show
                // going away means the system closed the keyboard.
                if (m_isShowing)
                {
                    Log.i(s_tag, "KeyboardHandler ime hidden by system, flag cleared");
                    m_isShowing = false;
                    m_showPending = false;
                    removeCallbacks(m_showTimeout);
                }
                return false;
            }

            if (!m_isShowing)
            {
                m_isShowing = true;
                m_showPending = false;
                Log.i(s_tag, "KeyboardHandler ime visible h=" + height);
            }
            removeCallbacks(m_showTimeout);
            return true;
        }

        private final Runnable m_confirmIme = new Runnable() {
            @Override
            public void run()
            {
                if (getVisibility() != View.VISIBLE)
                {
                    return;
                }

                View decor = GetView();
                WindowInsets insets = decor != null ? decor.getRootWindowInsets() : null;
                if (insets == null)
                {
                    insets = getRootWindowInsets();
                }
                noteIme(insets);
            }
        };

        private final Runnable m_showTimeout = new Runnable() {
            @Override
            public void run()
            {
                if (m_showPending)
                {
                    m_showPending = false;
                    m_isShowing = false;
                    Log.w(s_tag, "KeyboardHandler ime show failed reason=timeout will_retry");
                }
            }
        };

        private volatile boolean m_isShowing;
        private volatile boolean m_showPending;
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
