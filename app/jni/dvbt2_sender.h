//
// Created by Rakuh on 9/26/2025.
//

#ifndef NAMIDTVBT2EXAMPLE_DVBT2_SENDER_H
#define NAMIDTVBT2EXAMPLE_DVBT2_SENDER_H

#include <string.h>
#include <stdint.h>
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <pthread.h>
#include <unistd.h>

GST_DEBUG_CATEGORY_STATIC (debug_category);

#define GST_CAT_DEFAULT debug_category

/*
 * These macros provide a way to store the native pointer to CustomData, which might be 32 or 64 bits, into
 * a jlong, which is always 64 bits, without warnings.
 */
#if GLIB_SIZEOF_VOID_P == 8

# define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(*env)->GetLongField (env, thiz, fieldID)
# define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField (env, thiz, fieldID, (jlong)data)
#else
# define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(jint)(*env)->GetLongField (env, thiz, fieldID)
# define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField (env, thiz, fieldID, (jlong)(jint)data)
#endif

#define TAG "dvbt2_sender"
/**
 * Have 2 tee line with
 * Queue name for each tee is "tee1", "tee2"
 * Output for each sync is "vsink1", "vsink2"
 */
#define VSYNC_0 "vsink0"
#define VSYNC_1 "vsink1"
#define VALVE   "valve"
#define UDP_VIDEO_SINK "v_udp_sink"
#define UDP_AUDIO_SINK "a_udp_sink"

#define PIPELINE_NAMI_VIDEOTEST "gltestsrc ! glupload ! " \
    /* Raise source framerate cap to 30fps (if camera supports it). */ \
    "tee name=t " \
    /* FMMD preview branch: same idea */ \
    "t. ! queue name=t0 ! " \
    "glcolorconvert ! glimagesink name="VSYNC_0" " \
    /* DW preview branch: small, leaky queue keeps UI responsive */ \
    "t. ! queue name=t1 ! " \
    "glcolorconvert ! glimagesink name="VSYNC_1" " \
    /* Multi up sink for the registed ip address */ \
    "t. ! queue name=t2 ! " \
    "glcolorconvert ! gldownload ! video/x-raw,format=I420 ! x264enc tune=zerolatency ! rtph264pay ! " \
    "multiudpsink name="UDP_VIDEO_SINK" sync=true async=false " \
    "openslessrc ! audioconvert ! voaacenc ! rtpmp4gpay ! "        \
    "multiudpsink name="UDP_AUDIO_SINK" sync=true async=false "        \

#define PIPELINE_NAMI_DVBT2 "ahcsrc device=0 ! video/x-raw,width=1920,height=1080,framerate=30/1 ! " \
    /* Raise source framerate cap to 30fps (if camera supports it). */ \
    "tee name=t " \
    /* FMMD preview branch: same idea */ \
    "t. ! queue name=t0 ! " \
    "videoconvert ! glimagesink name="VSYNC_0" sync=false async=false " \
    /* DW preview branch: small, leaky queue keeps UI responsive */ \
    "t. ! queue name=t1 ! " \
    "videoconvert ! glimagesink name="VSYNC_1" sync=false async=false " \
    /* Multi up sink for the registed ip address */ \
    "t. ! queue name=t2 ! " \
    "videoconvert ! x264enc tune=zerolatency ! rtph264pay ! " \
    "multiudpsink name="UDP_VIDEO_SINK" sync=true async=false " \
    "openslessrc ! audioconvert ! voaacenc ! rtpmp4gpay ! " \
    "multiudpsink name="UDP_AUDIO_SINK" sync=true async=false " \

// GSurface
#define SURFACE_FMMW 0
#define SURFACE_DW   1
#define SURFACE_MAX  2
// Connection

typedef struct _Surface {
    GstElement* video_sink;
    ANativeWindow* native_window;
    bool active;
} Surface;

typedef enum _CustomElementEnum {
    E_CE_UDP_VIDEO_SINK,
    E_CE_UDP_AUDIO_SINK,
    E_CE_VALVE,
    E_CE_VALVE_TRIGGER,
    E_CE_VSYNC_0,
    E_CE_VSYNC_1,
    E_CE_MAX,
} CustomElementEnum;

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData
{
    jobject app;                  /* Application instance, used to call its methods. A global reference is kept. */
    GstElement *pipeline;         /* The running pipeline */
    GMainContext *context;        /* GLib context used to run the main loop */
    GMainLoop *main_loop;         /* GLib main loop */
    gboolean initialized;         /* To avoid informing the UI multiple times about the initialization */
    Surface surface[SURFACE_MAX]; /* Application Surfaces List */
    GstElement *element[E_CE_MAX];
    gboolean vsink_state[2];
    gboolean testmode;
} CustomData;

/* Custom data pointer which will be save from application zone */
static jfieldID custom_data_field_id;

/* Private methods
 * All Methods follow only call internal (inside) (not open for all). Don't export it out to NATIVE call
 */

/* Register this thread with the VM */
static JNIEnv * attach_current_thread (void);

/* Unregister this thread from the VM */
static void detach_current_thread (void *env);

/* Retrieve the JNI environment for this thread */
static JNIEnv * get_jni_env (void);

/* Change the content of the UI's TextView */
static void set_ui_message (const gchar * message, CustomData * data);

/* Retrieve errors from the bus and show them on the UI */
static void error_cb (GstBus * bus, GstMessage * msg, CustomData * data);

/* Notify UI about pipeline state changes */
static void state_changed_cb (GstBus * bus, GstMessage * msg, CustomData * data);

/* Check if all conditions are met to report GStreamer as initialized. */
static void check_initialization_complete (CustomData * data);

/* WARNING: Main method for the native code. This is executed on its own thread. */
static void * app_function (void *userdata);

/*
 * Java Bindings
 */

/* Instruct the native code to create its internal data structure, pipeline and thread */
static void gst_native_init (JNIEnv * env, jobject thiz);

/* Quit the main loop, remove the native thread and free resources */
static void gst_native_finalize (JNIEnv * env, jobject thiz);

/* Set pipeline to PLAYING state */
static void gst_native_play (JNIEnv * env, jobject thiz);

/* Set pipeline to PAUSED state */
static void gst_native_pause (JNIEnv * env, jobject thiz);

/* Static class initializer: retrieve method and field IDs */
static jboolean gst_native_class_init (JNIEnv * env, jclass klass);

static void gst_native_surface_init (JNIEnv * env, jobject thiz, jint id, jobject surface);

static void gst_native_surface_finalize (JNIEnv * env, jobject thiz, jint id);

/* Change the ui state */
static void gst_native_add_client (JNIEnv * env, jobject thiz, jstring ip, jint port);

/* Change the ui state */
static void gst_native_remove_client (JNIEnv * env, jobject thiz, jstring ip, jint port);

static void gst_native_start_broadcast (JNIEnv * env, jobject thiz, jstring ip, jint port);

static void gst_native_stop_broadcast (JNIEnv * env, jobject thiz, jstring ip, jint port);

static void gst_native_start_videotestsrc (JNIEnv * env, jobject thiz);

static void gst_native_stop_videotestsrc (JNIEnv * env, jobject thiz);

typedef enum _Method
{
    METHOD_GST_MESSAGE,     // This for method send back the message to the application (maybe unused or for debug)
    METHOD_GST_INITIALIZED, // This method notify for gstreamer initialized successfully
    METHOD_GST_STATE,       // This method notify for gstreamer initialized successfully
    METHOD_ID_MAX,
} Method;

/*
 * List of implemented callback method
 * */
static jmethodID Jmethod[METHOD_ID_MAX];
#endif //NAMIDTVBT2EXAMPLE_DVBT2_SENDER_H
