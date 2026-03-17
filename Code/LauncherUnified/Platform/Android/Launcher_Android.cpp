/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <Launcher.h>

#include <../Common/UnixLike/Launcher_UnixLike.h>

#include <AzCore/Android/AndroidEnv.h>
#include <AzCore/Android/Utils.h>
#include <AzCore/Android/JNI/JNI.h>
#include <AzCore/Android/JNI/Object.h>
#include <AzCore/Android/JNI/scoped_ref.h>

#include <AzFramework/API/ApplicationAPI_Platform.h>
#include <AzFramework/Input/Buses/Notifications/RawInputNotificationBus_Platform.h>
#include <AzGameFramework/Application/GameApplication.h>

#include <IConsole.h>

#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <android_native_app_glue.h>

#include <sys/resource.h>
#include <sys/types.h>
#include <AzCore/std/containers/deque.h>
#include <pthread.h>

#if defined(AZ_ENABLE_TRACING) || defined(RELEASE_LOGGING)
    #define ENABLE_LOGGING
#endif // defined(AZ_ENABLE_TRACING) || defined(RELEASE_LOGGING)

#if defined(ENABLE_LOGGING)
    #define LOG_TAG "LMBR"
    #define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
    #define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
    #define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

    struct COutputPrintSink
        : public IOutputPrintSink
    {
        virtual void Print(const char* message)
        {
            LOGI("%s", message);
        }
    };

    COutputPrintSink g_androidPrintSink;
#else
    #define LOGI(...)
    #define LOGE(...)
#endif // !defined(_RELEASE)


#define MAIN_EXIT_FAILURE(_appState, ...) \
    LOGE("****************************************************************"); \
    LOGE("STARTUP FAILURE - EXITING"); \
    LOGE("REASON:"); \
    LOGE(__VA_ARGS__); \
    LOGE("****************************************************************"); \
    _appState->userData = nullptr; \
    ANativeActivity_finish(_appState->activity); \
    while (_appState->destroyRequested == 0) { \
        g_eventDispatcher.PumpAllEvents(); \
    } \
    return;


namespace
{
    class NativeEventDispatcher
        : public AzFramework::AndroidEventDispatcher
    {
    public:
        NativeEventDispatcher()
            : m_appState(nullptr)
        {
        }

        ~NativeEventDispatcher() = default;

        void PumpAllEvents() override
        {
            auto androidEnv = static_cast<AZ::Android::AndroidEnv*>(m_appState->userData);

            if (!androidEnv)
            {
                LOGE("androidEnv is null");
                return;
            }

            if (!androidEnv->IsRunning())
            {
                while (!androidEnv->IsRunning())
                {
                    usleep(100000);
                    ProcessAllEvents();
                }
            }
            else
            {
                ProcessAllEvents();
            }
        }

        void PumpEventLoopOnce() override
        {
            AZStd::function<void()> event;
            {
                AZStd::lock_guard<AZStd::mutex> lock(m_eventQueueMutex);
                if (!m_eventQueue.empty())
                {
                    event = AZStd::move(m_eventQueue.front());
                    m_eventQueue.pop_front();
                }
            }

            if (event)
            {
                event();
                return;
            }
        }

        void ProcessAllEvents()
        {
            AZStd::deque<AZStd::function<void()>> localQueue;
            {
                AZStd::lock_guard<AZStd::mutex> lock(m_eventQueueMutex);
                if (!m_eventQueue.empty())
                {
                    localQueue = AZStd::move(m_eventQueue);
                    m_eventQueue.clear();
                }
            }

            for (auto& event : localQueue)
            {
                event();
            }
        }

        void QueueEvent(AZStd::function<void()> event)
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_eventQueueMutex);
            m_eventQueue.push_back(AZStd::move(event));
        }

        void SetAppState(android_app* appState)
        {
            m_appState = appState;
        }

    private:
        // signature of ALooper_pollOnce and ALooper_pollAll -> int timeoutMillis, int* outFd, int* outEvents, void** outData
        typedef int (*EventPumpFunc)(int, int*, int*, void**);

        android_app* m_appState;
        AZStd::deque<AZStd::function<void()>> m_eventQueue;
        AZStd::mutex m_eventQueueMutex;
    };

    NativeEventDispatcher g_eventDispatcher;
    std::atomic<bool> g_windowInitialized = false;

    pthread_t g_androidEventThread;
    std::atomic<bool> g_isRequestingExit = false;
    std::atomic<bool> g_eventHandlerInitialized = false;

    pthread_mutex_t g_eventMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t g_eventCond = PTHREAD_COND_INITIALIZER;

    void OnPostAppStart()
    {
        // set the event dispatcher with the application framework
        AzFramework::AndroidAppRequests::Bus::Broadcast(&AzFramework::AndroidAppRequests::SetEventDispatcher, &g_eventDispatcher);

        // queue the dismissal of the system splash screen in case the engine splash is disabled
        AZ::TickBus::QueueFunction([](){
            AZ::Android::Utils::DismissSplashScreen();
        });
    }

    int32_t HandleInputEvents(android_app* app, AInputEvent* event)
    {
        g_eventDispatcher.QueueEvent([event](){
            AzFramework::RawInputNotificationBusAndroid::Broadcast(&AzFramework::RawInputNotificationsAndroid::OnRawInputEvent, event);
        });
        return 0;
    }

    void HandleApplicationLifecycleEvents(android_app* appState, int32_t command)
    {
    #if defined(ENABLE_LOGGING)
        const char* commandNames[] = {
            "APP_CMD_INPUT_CHANGED",
            "APP_CMD_INIT_WINDOW",
            "APP_CMD_TERM_WINDOW",
            "APP_CMD_WINDOW_RESIZED",
            "APP_CMD_WINDOW_REDRAW_NEEDED",
            "APP_CMD_CONTENT_RECT_CHANGED",
            "APP_CMD_GAINED_FOCUS",
            "APP_CMD_LOST_FOCUS",
            "APP_CMD_CONFIG_CHANGED",
            "APP_CMD_LOW_MEMORY",
            "APP_CMD_START",
            "APP_CMD_RESUME",
            "APP_CMD_SAVE_STATE",
            "APP_CMD_PAUSE",
            "APP_CMD_STOP",
            "APP_CMD_DESTROY",
        };
        if (command >= 0 && command < sizeof(commandNames))
        {
            LOGI("Engine command received: %s", commandNames[command]);
        }
        else
        {
            LOGW("Unknown engine command received: %d", command);
        }
    #endif

        AZ::Android::AndroidEnv* androidEnv = static_cast<AZ::Android::AndroidEnv*>(appState->userData);
        if (!androidEnv)
        {
            return;
        }

        switch (command)
        {
            case APP_CMD_GAINED_FOCUS:
            {
#if defined(CARBONATED)
                androidEnv->SetIsRunning(true);
#endif
                g_eventDispatcher.QueueEvent([](){
                    AzFramework::AndroidLifecycleEvents::Bus::Broadcast(
                            &AzFramework::AndroidLifecycleEvents::Bus::Events::OnGainedFocus);
                });
            }
            break;

            case APP_CMD_LOST_FOCUS:
            {
                g_eventDispatcher.QueueEvent([]() {
                    AzFramework::AndroidLifecycleEvents::Bus::Broadcast(
                            &AzFramework::AndroidLifecycleEvents::Bus::Events::OnLostFocus);
                });
            }
            break;

            case APP_CMD_PAUSE:
            {
                androidEnv->SetIsRunning(false);
                g_eventDispatcher.QueueEvent([](){
                    AzFramework::AndroidLifecycleEvents::Bus::Broadcast(
                        &AzFramework::AndroidLifecycleEvents::Bus::Events::OnPause);
                });
            }
            break;

            case APP_CMD_RESUME:
            {
#if defined(CARBONATED)
                // moved to APP_CMD_GAINED_FOCUS
#else
                androidEnv->SetIsRunning(true);
#endif
                g_eventDispatcher.QueueEvent([](){
                    AzFramework::AndroidLifecycleEvents::Bus::Broadcast(
                            &AzFramework::AndroidLifecycleEvents::Bus::Events::OnResume);
                });
            }
            break;

            case APP_CMD_DESTROY:
            {
                g_eventDispatcher.QueueEvent([](){
                    AzFramework::AndroidLifecycleEvents::Bus::Broadcast(
                        &AzFramework::AndroidLifecycleEvents::Bus::Events::OnDestroy);
                });
            }
            break;

            case APP_CMD_INIT_WINDOW:
            {
                g_eventDispatcher.QueueEvent([androidEnv, appState](){
                    g_windowInitialized = true;
                    androidEnv->SetWindow(appState->window);
                    AzFramework::AndroidLifecycleEvents::Bus::Broadcast(
                        &AzFramework::AndroidLifecycleEvents::Bus::Events::OnWindowInit);
                });
            }
            break;

            case APP_CMD_TERM_WINDOW:
            {
                g_eventDispatcher.QueueEvent([androidEnv](){
                    AzFramework::AndroidLifecycleEvents::Bus::Broadcast(
                        &AzFramework::AndroidLifecycleEvents::Bus::Events::OnWindowDestroy);
                    androidEnv->SetWindow(nullptr);
                });
#if defined(CARBONATED)
                // On some devices, in some cases, the APP_CMD_TERM_WINDOW
                // message may arrive before APP_CMD_PAUSE
                androidEnv->SetIsRunning(false);
#endif
            }
            break;

            case APP_CMD_LOW_MEMORY:
            {
                g_eventDispatcher.QueueEvent([](){
                    AzFramework::AndroidLifecycleEvents::Bus::Broadcast(
                        &AzFramework::AndroidLifecycleEvents::Bus::Events::OnLowMemory);
                });
            }
            break;

            case APP_CMD_CONFIG_CHANGED:
            {
                g_eventDispatcher.QueueEvent([androidEnv](){
                    androidEnv->UpdateConfiguration();
                });
            }
            break;

            case APP_CMD_WINDOW_REDRAW_NEEDED:
            {
                g_eventDispatcher.QueueEvent([](){
                AzFramework::AndroidLifecycleEvents::Bus::Broadcast(
                    &AzFramework::AndroidLifecycleEvents::Bus::Events::OnWindowRedrawNeeded);
                });
            }
            break;
        }
    }

    void OnWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* rect)
    {
        auto app = static_cast<android_app*>(activity->instance);
        int8_t cmd = APP_CMD_WINDOW_REDRAW_NEEDED;
        if (write(app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd))
        {
            LOGE("Failure writing android_app cmd: %s\n", strerror(errno));
        }
    }
}


static void* AndroidEventThreadWorker(void* param)
{
    pthread_setname_np(pthread_self(), "AndroidEventWorker");

    struct android_app* state = (struct android_app*)param;

    ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    ALooper_addFd(looper, state->msgread, LOOPER_ID_MAIN, ALOOPER_EVENT_INPUT, NULL, &state->cmdPollSource);
    state->looper = looper;
    state->onAppCmd = HandleApplicationLifecycleEvents;
    state->onInputEvent = HandleInputEvents;

    LOGI("Event thread started");

    pthread_mutex_lock(&g_eventMutex);
    g_eventHandlerInitialized = true;
    pthread_cond_broadcast(&g_eventCond);
    pthread_mutex_unlock(&g_eventMutex);

    LOGI("Event handler initialized, starting event loop");

    while (!g_isRequestingExit)
    {
        auto androidEnv = static_cast<AZ::Android::AndroidEnv*>(state->userData);
        if (!androidEnv)
        {
            break;
        }

        int32_t eventsProcessed = 0;
        const int32_t maxEventsPerFrame = 10;

        while (eventsProcessed < maxEventsPerFrame)
        {
            struct android_poll_source* source = nullptr;
            int32_t result = ALooper_pollOnce(0, nullptr, nullptr, (void**)&source);

            if (result == ALOOPER_POLL_ERROR || source == nullptr)
            {
                LOGE("AndroidEventThreadWorker - ALooper pool error");
                break;
            }

            source->process(state, source);
            eventsProcessed++;
        }

        if (eventsProcessed == 0)
        {
            usleep(androidEnv->IsRunning() ? 16000 : 100000);
        }
    }

    LOGI("Event thread exiting");
    return nullptr;
}

static void SetupAndroidEnv(android_app* appState)
{
    AZ::Android::AndroidEnv::Descriptor descriptor;

    descriptor.m_jvm = appState->activity->vm;
    descriptor.m_activityRef = appState->activity->clazz;
    descriptor.m_assetManager = appState->activity->assetManager;

    descriptor.m_configuration = appState->config;

    descriptor.m_appPrivateStoragePath = appState->activity->internalDataPath;
    descriptor.m_appPublicStoragePath = appState->activity->externalDataPath;
    descriptor.m_obbStoragePath = appState->activity->obbPath;

    if (!AZ::Android::AndroidEnv::Create(descriptor))
    {
        AZ::Android::AndroidEnv::Destroy();
        MAIN_EXIT_FAILURE(appState, "Failed to create the AndroidEnv");
    }

    AZ::Android::AndroidEnv* androidEnv = AZ::Android::AndroidEnv::Get();
    appState->userData = androidEnv;
    androidEnv->SetIsRunning(true);
}

static void StartPoolUpdateLoop(android_app* appState)
{
    LOGI("Looper prepared and fd registered");

    pthread_attr_t threadAttr;
    pthread_attr_init(&threadAttr);
    pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_JOINABLE);

    int createResult = pthread_create(&g_androidEventThread, &threadAttr, AndroidEventThreadWorker, appState);

    if (createResult != 0)
    {
        LOGE("Failed to create event thread: %d", createResult);
        MAIN_EXIT_FAILURE(appState, "Failed to create Android event thread");
    }

    pthread_attr_destroy(&threadAttr);

    LOGI("Event thread created, waiting for initialization...");

    // Wait for event handler to be initialized
    pthread_mutex_lock(&g_eventMutex);
    while (!g_eventHandlerInitialized)
    {
        pthread_cond_wait(&g_eventCond, &g_eventMutex);
    }
    pthread_mutex_unlock(&g_eventMutex);

    LOGI("Event handler initialized");
}

static void StopPoolUpdateLoop(android_app* appState)
{
    g_isRequestingExit = true;

    if (appState->looper)
    {
        ALooper_wake(appState->looper);
    }

    // Wait for event thread to finish
    pthread_join(g_androidEventThread, nullptr);

    // Cleanup synchronization objects
    pthread_mutex_destroy(&g_eventMutex);
    pthread_cond_destroy(&g_eventCond);

    LOGI("Event thread joined, cleaning up Android environment");
}

// This is the main entry point of a native application that is using android_native_app_glue.
// It runs in its own thread, with its own event loop for receiving input events
void android_main(android_app* appState)
{
    const AZ::Debug::Trace tracer;
    // Adding a start up banner so you can see when the game is starting up in amongst the logcat spam
    LOGI("****************************************************************");
    LOGI("*                      Launching Game...                       *");
    LOGI("****************************************************************");

    g_eventDispatcher.SetAppState(appState);

    // This callback will notify us when the orientation of the device changes.
    // While Android does have an onNativeWindowResized callback, it is never called in android_native_app_glue when the window size changes.
    // The onNativeConfigChanged callback is called too early(before the window size has changed), so we won't have the correct window size at that point.
    appState->activity->callbacks->onNativeWindowRedrawNeeded = OnWindowRedrawNeeded;

    SetupAndroidEnv(appState);
    StartPoolUpdateLoop(appState);

    while (!g_windowInitialized)
    {
        g_eventDispatcher.ProcessAllEvents();
        usleep(16000);
    }

    // Now that the window has been created we can show the java splash screen.  We need
    // to do it here and not in the window init event because every time the app is
    // backgrounded/foregrounded the window is destroyed/created, respectively.  So, we
    // don't want to show the splash screen when we resumed from a paused state.
    AZ::Android::Utils::ShowSplashScreen();

    // run the Lumberyard application
    using namespace O3DELauncher;

    PlatformMainInfo mainInfo;
    mainInfo.m_updateResourceLimits = IncreaseResourceLimits;
    mainInfo.m_onPostAppStart = OnPostAppStart;
    mainInfo.m_appResourcesPath = AZ::Android::Utils::FindAssetsDirectory();
    mainInfo.m_additionalVfsResolution = "\t- Make sure \'adb reverse\' is setup for the device when connecting to localhost";

    // Always add the app as the first arg to mimic the way other platforms start with the executable name.
    const char* packageName = AZ::Android::Utils::GetPackageName();
    if (packageName)
    {
        mainInfo.AddArgument(packageName);
    }

    // Get the string extras and pass them along as cmd line params
    AZ::Android::JNI::Internal::Object<AZ::OSAllocator> activityObject(AZ::Android::JNI::GetEnv()->GetObjectClass(appState->activity->clazz), appState->activity->clazz);

    activityObject.RegisterMethod("getIntent", "()Landroid/content/Intent;");
    jobject intent = activityObject.InvokeObjectMethod<jobject>("getIntent");

    AZ::Android::JNI::Internal::Object<AZ::OSAllocator> intentObject(AZ::Android::JNI::GetEnv()->GetObjectClass(intent), intent);
    intentObject.RegisterMethod("getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
    intentObject.RegisterMethod("getExtras", "()Landroid/os/Bundle;");
    jobject extras = intentObject.InvokeObjectMethod<jobject>("getExtras");

    if (extras)
    {
        // Get the set of keys
        AZ::Android::JNI::Internal::Object<AZ::OSAllocator> extrasObject(AZ::Android::JNI::GetEnv()->GetObjectClass(extras), extras);
        extrasObject.RegisterMethod("keySet", "()Ljava/util/Set;");
        jobject extrasKeySet = extrasObject.InvokeObjectMethod<jobject>("keySet");

        // get the array of string objects
        AZ::Android::JNI::Internal::Object<AZ::OSAllocator> extrasKeySetObject(AZ::Android::JNI::GetEnv()->GetObjectClass(extrasKeySet), extrasKeySet);
        extrasKeySetObject.RegisterMethod("toArray", "()[Ljava/lang/Object;");
        jobjectArray extrasKeySetArray = extrasKeySetObject.InvokeObjectMethod<jobjectArray>("toArray");

        int extrasKeySetArraySize = AZ::Android::JNI::GetEnv()->GetArrayLength(extrasKeySetArray);

        for (int x = 0; x < extrasKeySetArraySize; x++)
        {
            jstring keyObject = static_cast<jstring>(AZ::Android::JNI::GetEnv()->GetObjectArrayElement(extrasKeySetArray, x));
            AZ::OSString value = intentObject.InvokeStringMethod("getStringExtra", keyObject);

            const char* keyChars = AZ::Android::JNI::GetEnv()->GetStringUTFChars(keyObject, 0);

            char argName[AZ_COMMAND_LINE_LEN] = { 0 };
            azsprintf(argName, "-%s", keyChars);
            mainInfo.AddArgument(argName);
            mainInfo.AddArgument(value.c_str());

            AZ::Android::JNI::GetEnv()->ReleaseStringUTFChars(keyObject, keyChars);
        }
    }

#if defined(_RELEASE)
    mainInfo.m_appWriteStoragePath = AZ::Android::Utils::GetAppPrivateStoragePath();
#else
    mainInfo.m_appWriteStoragePath = AZ::Android::Utils::GetAppPublicStoragePath();
#endif // defined(_RELEASE)

#if defined(ENABLE_LOGGING)
    mainInfo.m_printSink = &g_androidPrintSink;
#endif // defined(ENABLE_LOGGING)

    ReturnCode status = Run(mainInfo);
    StopPoolUpdateLoop(appState);
    AZ::Android::AndroidEnv::Destroy();

    if (status != ReturnCode::Success)
    {
        MAIN_EXIT_FAILURE(appState, "%s", GetReturnCodeString(status));
    }
}

void CVar_OnViewportPosition([[maybe_unused]] const AZ::Vector2& value) {}
