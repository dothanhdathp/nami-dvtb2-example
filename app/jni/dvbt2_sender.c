#include "dvbt2_sender.h"

/* These global variables cache values which are not changing during execution */
static pthread_t gst_app_thread;
static pthread_key_t current_jni_env;
static JavaVM *java_vm;

// static jmethodID set_message_method_id;
// static jmethodID on_gstreamer_initialized_method_id;

/*
 * Private methods
 */

/* Register this thread with the VM */
static JNIEnv * attach_current_thread (void)
{
    JNIEnv *env;
    JavaVMAttachArgs args;

    GST_DEBUG ("Attaching thread %p", g_thread_self ());
    args.version = JNI_VERSION_1_4;
    args.name = NULL;
    args.group = NULL;

    if ((*java_vm)->AttachCurrentThread (java_vm, &env, &args) < 0) {
        GST_ERROR ("Failed to attach current thread");
        return NULL;
    }

    return env;
}

/* Unregister this thread from the VM */
static void detach_current_thread (void *env)
{
    GST_DEBUG ("Detaching thread %p", g_thread_self ());
    (*java_vm)->DetachCurrentThread (java_vm);
}

/* Retrieve the JNI environment for this thread */
static JNIEnv * get_jni_env (void)
{
    JNIEnv *env;

    if ((env = pthread_getspecific (current_jni_env)) == NULL) {
        env = attach_current_thread ();
        pthread_setspecific (current_jni_env, env);
    }

    return env;
}

/* Change the content of the UI's TextView */
static void set_ui_message (const gchar * message, CustomData * data)
{
    JNIEnv *env = get_jni_env ();
    GST_DEBUG ("Setting message to: %s", message);
    jstring jmessage = (*env)->NewStringUTF (env, message);
    (*env)->CallVoidMethod (env, data->app, Jmethod[METHOD_GST_MESSAGE], jmessage);
    if ((*env)->ExceptionCheck (env)) {
        GST_ERROR ("Failed to call Java method");
        (*env)->ExceptionClear (env);
    }
    (*env)->DeleteLocalRef (env, jmessage);
}

/* Change the ui state */
static void set_ui_state (GstState state, CustomData * data)
{
    JNIEnv *env = get_jni_env ();
    jint jstate = state;
    (*env)->CallVoidMethod (env, data->app, Jmethod[METHOD_GST_STATE], jstate);
    if ((*env)->ExceptionCheck (env)) {
        GST_ERROR ("Failed to call Java method");
        (*env)->ExceptionClear (env);
    }
}

/* Retrieve errors from the bus and show them on the UI */
static void error_cb (GstBus * bus, GstMessage * msg, CustomData * data)
{
    GError *err;
    gchar *debug_info;
    gchar *message_string;

    gst_message_parse_error (msg, &err, &debug_info);
    message_string = g_strdup_printf ("Error received from element %s: %s", GST_OBJECT_NAME (msg->src), err->message);
    g_clear_error (&err);
    g_free (debug_info);
    set_ui_message (message_string, data);
    g_free (message_string);
    gst_element_set_state (data->pipeline, GST_STATE_NULL);
    set_ui_state(GST_STATE_NULL, data);
}

/* Notify UI about pipeline state changes */
static void state_changed_cb (GstBus * bus, GstMessage * msg, CustomData * data)
{
    GstState old_state, new_state, pending_state;
    /* Only pay attention to messages coming from the pipeline, not its children */
    if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_STATE_CHANGED) {
        // State changed
        gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);

        if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->pipeline)) {
            gchar *message = g_strdup_printf ("Pipeline state [%s] -> [%s] : {%s}",
                                              gst_element_state_get_name (old_state),
                                              gst_element_state_get_name (new_state),
                                              gst_element_state_get_name (pending_state));
            set_ui_state(new_state, data);
            g_free (message);
        } else {
            // GST_DEBUG ("Object state [%s] :: [%s] -> [%s] : {%s}",
            //            GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)),
            //            gst_element_state_get_name (old_state),
            //            gst_element_state_get_name (new_state),
            //            gst_element_state_get_name (pending_state));
            if(GST_MESSAGE_SRC(msg) == GST_OBJECT (data->element[E_CE_VSYNC_0])) {
                data->vsink_state[0] = new_state;
                if((GST_STATE_PLAYING == data->vsink_state[0]) && (GST_STATE_PLAYING == data->vsink_state[1]))
                    set_ui_state(GST_STATE_PLAYING, data);
            }
            if(GST_MESSAGE_SRC(msg) == GST_OBJECT (data->element[E_CE_VSYNC_1])) {
                data->vsink_state[1] = new_state;
                if((GST_STATE_PLAYING == data->vsink_state[0]) && (GST_STATE_PLAYING == data->vsink_state[1]))
                    set_ui_state(GST_STATE_PLAYING, data);
            }

        }
    }
}

/** Check if all conditions are met to report GStreamer as initialized.
 * Need one
 * */
static void check_initialization_complete (CustomData * data)
{
    JNIEnv *env = get_jni_env ();
    if (!data->initialized && data->main_loop) {
        for(int id=SURFACE_FMMW; id<SURFACE_MAX; ++id) {
            if(data->surface[id].native_window) {
                GST_DEBUG ("Initialization complete, notifying application. native_window:%p main_loop:%p", data->surface[id].native_window, data->main_loop);
                gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->surface[id].video_sink), (guintptr) data->surface[id].native_window);
                data->initialized = TRUE;
            }
        }

        if(data->initialized) {
            (*env)->CallVoidMethod (env, data->app, Jmethod[METHOD_GST_INITIALIZED]);
            if ((*env)->ExceptionCheck (env)) {
                (*env)->ExceptionClear (env);
            }
        }
    } else {
        GST_ERROR("Initialization failed [%d][%d][%d]", data->initialized, data->surface[SURFACE_FMMW].native_window, data->main_loop);
    }
}

/* Main method for the native code. This is executed on its own thread. */
static void * app_function (void *userdata)
{
    JavaVMAttachArgs args;
    GstBus *bus;
    CustomData *data = (CustomData *) userdata;
    GSource *bus_source;
    GError *error = NULL;

    GST_DEBUG ("Creating pipeline in CustomData at %p", data);

    /* Create our own GLib Main Context and make it the default one */
    data->context = g_main_context_new ();
    g_main_context_push_thread_default (data->context);

    /* Build pipeline */
    // data->pipeline = gst_parse_launch (PIPELINE_NAMI_VIDEOTEST, &error);

    if(data->testmode) {
        GST_DEBUG ("PIPELINE: "PIPELINE_NAMI_VIDEOTEST);
        data->pipeline = gst_parse_launch (PIPELINE_NAMI_VIDEOTEST, &error);
    } else {
        GST_DEBUG ("PIPELINE: "PIPELINE_NAMI_DVBT2);
        data->pipeline = gst_parse_launch (PIPELINE_NAMI_DVBT2, &error);
    }

    if (error) {
        gchar *message = g_strdup_printf ("Unable to build pipeline: %s", error->message);
        g_clear_error (&error);
        set_ui_message (message, data);
        g_free (message);
        return NULL;
    }

    /* Init Pipeline */
    data->element[E_CE_UDP_VIDEO_SINK] = gst_bin_get_by_name(GST_BIN(data->pipeline), UDP_VIDEO_SINK);
    data->element[E_CE_UDP_AUDIO_SINK] = gst_bin_get_by_name(GST_BIN(data->pipeline), UDP_AUDIO_SINK);

    data->element[E_CE_VALVE] = gst_bin_get_by_name(GST_BIN(data->pipeline), VALVE);
    g_object_set(G_OBJECT(data->element[E_CE_VALVE_TRIGGER]), "drop", TRUE, NULL);
    data->element[E_CE_VSYNC_0] = gst_bin_get_by_name(GST_BIN(data->pipeline), VSYNC_0);
    data->element[E_CE_VSYNC_1] = gst_bin_get_by_name(GST_BIN(data->pipeline), VSYNC_1);
    data->vsink_state[0] = false;
    data->vsink_state[0] = false;

    /* Set the pipeline to READY, so it can already accept a window handle, if we have one */
    gst_element_set_state (data->pipeline, GST_STATE_READY);

    // data->surface[SURFACE_FMMW].video_sink = gst_bin_get_by_interface (GST_BIN (data->pipeline), GST_TYPE_VIDEO_OVERLAY);

    // TO DO: I want this can expand for any count of screnn which just need 'add' into this. But any way pls make it late. Another module still follow that kind.
    data->surface[SURFACE_FMMW].video_sink = gst_bin_get_by_name(GST_BIN (data->pipeline), VSYNC_0);
    data->surface[SURFACE_DW].video_sink = gst_bin_get_by_name(GST_BIN (data->pipeline), VSYNC_1);

    if (!data->surface[SURFACE_FMMW].video_sink && !data->surface[SURFACE_DW].video_sink) {
        GST_ERROR ("Could not retrieve video sink");
        return NULL;
    }

    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    bus = gst_element_get_bus (data->pipeline);
    bus_source = gst_bus_create_watch (bus);
    g_source_set_callback (bus_source, (GSourceFunc) gst_bus_async_signal_func,NULL, NULL);
    g_source_attach (bus_source, data->context);
    g_source_unref (bus_source);
    g_signal_connect (G_OBJECT (bus), "message::error", (GCallback) error_cb, data);
    g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback) state_changed_cb, data);
    gst_object_unref (bus);

    /* Create a GLib Main Loop and set it to run */
    GST_DEBUG ("Entering main loop... (CustomData:%p)", data);
    data->main_loop = g_main_loop_new (data->context, FALSE);
    check_initialization_complete (data);
    g_main_loop_run (data->main_loop);
    GST_DEBUG ("Exited main loop");
    g_main_loop_unref (data->main_loop);
    data->main_loop = NULL;

    /* Free resources */
    g_main_context_pop_thread_default (data->context);
    g_main_context_unref (data->context);
    gst_element_set_state (data->pipeline, GST_STATE_NULL);
    gst_object_unref (data->surface[SURFACE_DW].video_sink);
    gst_object_unref (data->surface[SURFACE_FMMW].video_sink);
    for (int ce_item = 0; ce_item < E_CE_MAX; ++ce_item) {
        gst_object_unref(data->element[ce_item]);
    }
    gst_object_unref (data->pipeline);
    return NULL;
}

/*
 * Java Bindings
 */

/* Instruct the native code to create its internal data structure, pipeline and thread */
static void gst_native_init (JNIEnv * env, jobject thiz)
{
    CustomData *data = g_new0 (CustomData, 1);
    SET_CUSTOM_DATA (env, thiz, custom_data_field_id, data);
    GST_DEBUG_CATEGORY_INIT (debug_category, TAG, 0, "DVBT2-SENDER");
    // change log level for debug
    gst_debug_set_default_threshold(GST_LEVEL_ERROR);
    // set level debug for app
    gst_debug_set_threshold_for_name (TAG, GST_LEVEL_DEBUG);
    GST_DEBUG ("Created CustomData at %p", data);
    data->app = (*env)->NewGlobalRef (env, thiz);
    GST_DEBUG ("Created GlobalRef for app object at %p", data->app);
    data->initialized = FALSE;
    data->main_loop = NULL;
    for (int e_id = 0; e_id < E_CE_MAX; ++ e_id) {
        data->element[e_id] = NULL;
    }
    data->surface[SURFACE_FMMW].native_window = NULL;
    data->surface[SURFACE_DW].native_window = NULL;
    data->vsink_state[0] = FALSE;
    data->vsink_state[1] = FALSE;
    data->testmode = FALSE;
    GST_DEBUG ("Init/Preset few data");
    pthread_create (&gst_app_thread, NULL, &app_function, data);
}

/* Quit the main loop, remove the native thread and free resources */
static void gst_native_finalize (JNIEnv * env, jobject thiz)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;
    GST_DEBUG ("Quitting main loop...");
    g_main_loop_quit (data->main_loop);
    GST_DEBUG ("Waiting for thread to finish...");
    pthread_join (gst_app_thread, NULL);
    GST_DEBUG ("Deleting GlobalRef for app object at %p", data->app);
    (*env)->DeleteGlobalRef (env, data->app);
    GST_DEBUG ("Freeing CustomData at %p", data);
    g_free (data);
    SET_CUSTOM_DATA (env, thiz, custom_data_field_id, NULL);
    GST_DEBUG ("Done finalizing");
}

/* Set pipeline to PLAYING state */
static void gst_native_play (JNIEnv * env, jobject thiz)
{
    CustomData *data = GET_CUSTOM_DATA(env, thiz, custom_data_field_id);
    if (!data) return;

    GST_DEBUG ("Setting state to PLAYING");
    gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
}

/* Set pipeline to PAUSED state */
static void gst_native_pause (JNIEnv * env, jobject thiz)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;
    GST_DEBUG ("Setting state to PAUSED");
    gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
}

/* Static class initializer: retrieve method and field IDs */
static jboolean gst_native_class_init (JNIEnv * env, jclass klass)
{
    // Just save the data field CustomData to application context
    custom_data_field_id = (*env)->GetFieldID (env, klass, "nativeCustomData", "J");

    // Set callback method
    Jmethod[METHOD_GST_MESSAGE] = (*env)->GetMethodID (env, klass, "setMessage", "(Ljava/lang/String;)V");
    Jmethod[METHOD_GST_INITIALIZED] = (*env)->GetMethodID (env, klass, "onGStreamerInitialized", "()V");
    Jmethod[METHOD_GST_STATE] = (*env)->GetMethodID (env, klass, "onGStreamerState", "(I)V");

    // Check JNI method. If have at least one method which not avaible, return false
    for(int i = 0; i < METHOD_ID_MAX; ++i) {
        if(!Jmethod[i]) {
            __android_log_print (ANDROID_LOG_ERROR, TAG, "The calling class does not implement all necessary interface methods");
            return JNI_FALSE;
        }
    }
    return JNI_TRUE;
}

/**
 *
 * @param env no comment
 * @param thiz no comment
 * @param id surface id
 * @param surface surface pointer
 */
static void gst_native_surface_init (JNIEnv * env, jobject thiz, jint id, jobject surface)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;
    ANativeWindow *new_native_window = ANativeWindow_fromSurface (env, surface);
    GST_DEBUG ("Received surface %p (native window %p)", surface, new_native_window);

    if (data->surface[id].native_window) {
        ANativeWindow_release (data->surface[id].native_window);
        if (data->surface[id].native_window == new_native_window) {
            GST_DEBUG ("New native window is the same as the previous one %p", data->surface[id].native_window);
            if (data->surface[id].video_sink) {
                gst_video_overlay_expose (GST_VIDEO_OVERLAY (data->surface[id].video_sink));
            }
            return;
        } else {
            GST_DEBUG ("Released previous native window %p", data->surface[id].native_window);
            data->initialized = FALSE;
        }
    }
    data->surface[id].native_window = new_native_window;

    check_initialization_complete (data);
}

/**
 *
 * @param env: no comment
 * @param thiz: no comment
 * @param id: surface id
 */
static void gst_native_surface_finalize (JNIEnv * env, jobject thiz, jint id)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;

    GST_DEBUG ("Releasing Native Window %p", data->surface[id].native_window);

    if (data->surface[id].video_sink) {
        gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->surface[id].video_sink),(guintptr) NULL);
        gst_element_set_state (data->pipeline, GST_STATE_READY);
    }

    ANativeWindow_release (data->surface[id].native_window);
    data->surface[id].native_window = NULL;
    data->initialized = FALSE;
}

static void gst_native_add_client (JNIEnv * env, jobject thiz, jstring ip, jint port)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;

    const char *_ip = (*env)->GetStringUTFChars(env, ip, NULL);
    g_signal_emit_by_name(G_OBJECT(data->element[E_CE_UDP_VIDEO_SINK]), "add", _ip, port);
    g_signal_emit_by_name(G_OBJECT(data->element[E_CE_UDP_AUDIO_SINK]), "add", _ip, port+1);
    GST_DEBUG ("Add Client: %s:%d", _ip, port);
    (*env)->ReleaseStringUTFChars(env, ip, _ip);
}

static void gst_native_remove_client (JNIEnv * env, jobject thiz, jstring ip, jint port)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;

    const char *_ip = (*env)->GetStringUTFChars(env, ip, NULL);
    g_signal_emit_by_name(G_OBJECT(data->element[E_CE_UDP_VIDEO_SINK]), "remove", _ip, port);
    g_signal_emit_by_name(G_OBJECT(data->element[E_CE_UDP_AUDIO_SINK]), "remove", _ip, port+1);
    GST_DEBUG ("Remove Client: %s:%d", _ip, port);
    (*env)->ReleaseStringUTFChars(env, ip, _ip);
}

/* Change the ui state */
static void gst_native_clear_all_client (JNIEnv * env, jobject thiz)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;

    g_signal_emit_by_name(G_OBJECT(data->element[E_CE_UDP_VIDEO_SINK]), "clear");
    g_signal_emit_by_name(G_OBJECT(data->element[E_CE_UDP_AUDIO_SINK]), "clear");
}

static void gst_native_start_broadcast (JNIEnv * env, jobject thiz, jstring ip, jint port)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;

    // Set ip
    GST_DEBUG ("Feature not ready");
}

static void gst_native_stop_broadcast (JNIEnv * env, jobject thiz, jstring ip, jint port)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;

    // disable stream (turn off)
    GST_DEBUG ("Feature not ready");
}

static void gst_native_start_videotestsrc (JNIEnv * env, jobject thiz)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;

    GST_DEBUG ("Stop <gst_app_thread> main loop...");
    g_main_loop_quit (data->main_loop);
    GST_DEBUG ("Waiting for thread to finish...");
    pthread_join (gst_app_thread, NULL);
    GST_DEBUG ("Enable test mode...");
    data->testmode = TRUE;
    GST_DEBUG ("Restart <gst_app_thread>");
    pthread_create (&gst_app_thread, NULL, &app_function, data);
}

static void gst_native_stop_videotestsrc (JNIEnv * env, jobject thiz)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data)
        return;

    GST_DEBUG ("Stop <gst_app_thread> main loop...");
    g_main_loop_quit (data->main_loop);
    GST_DEBUG ("Waiting for thread to finish...");
    pthread_join (gst_app_thread, NULL);
    GST_DEBUG ("Disable test mode...");
    data->testmode = FALSE;
    GST_DEBUG ("Restart <gst_app_thread>");
    pthread_create (&gst_app_thread, NULL, &app_function, data);
}
/*
 * List of implemented native methods
 * */
static JNINativeMethod native_methods[] = {
        {"nativeInit", "()V", (void *) gst_native_init},
        {"nativeFinalize", "()V", (void *) gst_native_finalize},
        {"nativePlay", "()V", (void *) gst_native_play},
        {"nativePause", "()V", (void *) gst_native_pause},
        {"nativeSurfaceInit", "(ILjava/lang/Object;)V", (void *) gst_native_surface_init},
        {"nativeSurfaceFinalize", "(I)V", (void *) gst_native_surface_finalize},
        {"nativeClassInit", "()Z", (void *) gst_native_class_init},
        {"nativeAddClient", "(Ljava/lang/String;I)V", (void *) gst_native_add_client},
        {"nativeRemoveClient", "(Ljava/lang/String;I)V", (void *) gst_native_remove_client},
        {"nativeClearAllClient", "()V", (void *) gst_native_clear_all_client},
        {"nativeStartBroadcast", "(Ljava/lang/String;I)V", (void *) gst_native_start_broadcast},
        {"nativeStopBroadcast", "(Ljava/lang/String;I)V", (void *) gst_native_stop_broadcast},
        {"nativeStartVideoTest", "()V", (void *) gst_native_start_videotestsrc},
        {"nativeStopVideoTest", "()V", (void *) gst_native_stop_videotestsrc},
};

/* Library initializer */
jint JNI_OnLoad (JavaVM * vm, void *reserved)
{
    JNIEnv *env = NULL;

    java_vm = vm;

    if ((*vm)->GetEnv (vm, (void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        __android_log_print (ANDROID_LOG_ERROR, "dvbt2_sender","Could not retrieve JNIEnv");
        return 0;
    }
    jclass klass = (*env)->FindClass (env, "nami/example/dtvbt2/DvbSenderManager");
    (*env)->RegisterNatives (env, klass, native_methods, G_N_ELEMENTS (native_methods));

    pthread_key_create (&current_jni_env, detach_current_thread);

    return JNI_VERSION_1_4;
}