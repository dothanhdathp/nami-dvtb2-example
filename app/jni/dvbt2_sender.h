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
#include <gst/gst.h>
#include <gst/video/video.h>
#include <pthread.h>


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

/*
 * These macros belong to project, user define this
 */
#define TAG "dvbt2_sender"
#define PIPELINE_DESCRIPTION_EXAMPLE "ahcsrc ! video/x-raw,width=1280,height=720,framerate=30/1 ! videoflip method=clockwise ! glimagesink"
#define PIPELINE_DESCRIPTION_EXAMPLE_90 "ahcsrc ! video/x-raw,width=1280,height=720,framerate=30/1 ! videoflip method=clockwise ! glimagesink"
#define PIPELINE_DESCRIPTION_NAMI_DVBT2 "ahcsrc device=0 ! " \
    /* Raise source framerate cap to 30fps (if camera supports it). */  \
    "video/x-raw,width=1920,height=1080,framerate=30/1 ! "  \
    "queue name=src_q max-size-buffers=6 max-size-bytes=0 max-size-time=0 ! "  \
    "tee name=t "  \
    /* DW preview branch: small, leaky queue keeps UI responsive */ \
    "t. ! queue name=dw_q  max-size-buffers=2 max-size-bytes=0 max-size-time=0 "  \
    "leaky=downstream ! valve name=v_dw drop=false ! "  \
    "glupload ! glcolorconvert ! "  \
    "glimagesink name=surface_sink_dw  sync=false async=false qos=false "  \
    /* FMMD preview branch: same idea */ \
    "t. ! queue name=fmmd_q max-size-buffers=2 max-size-bytes=0 max-size-time=0 " \
    "leaky=downstream ! valve name=v_fmmd drop=false ! " \
    "glupload ! glcolorconvert ! " \
    "glimagesink name=surface_sink_fmmd sync=false async=false qos=false "

/**
 * Have 2 tee line with
 * Queue name for each tee is "tee1", "tee2"
 * Output for each sync is "vsink1", "vsink2"
 */
#define VSYNC_0 "vsink0"
#define VSYNC_1 "vsink1"
#define PIPELINE_DESCRIPTION_NAMI_DVBT2_90 "ahcsrc ! " \
    /* Raise source framerate cap to 30fps (if camera supports it). */ \
    "video/x-raw,width=1280,height=720,framerate=30/1 ! "\
    "tee name=t " \
    /* FMMD preview branch: same idea */ \
    "t. ! queue name=tee1 max-size-buffers=2 max-size-bytes=0 max-size-time=0 ! " \
    "videoflip method=clockwise ! " \
    "glimagesink name="VSYNC_0" " \
    /* DW preview branch: small, leaky queue keeps UI responsive */ \
    "t. ! queue name=tee2 max-size-buffers=2 max-size-bytes=0 max-size-time=0 ! " \
    "videoflip method=clockwise ! " \
    "glimagesink name="VSYNC_1

/* Define for Table */
// Id
#define TABLET_ID_LEFT 0
#define TABLET_ID_RIGHT 1
#define TABLET_ID_CENTER 2
#define TABLET_ID_MAX 3
// GSurface
#define SURFACE_FMMW 0
#define SURFACE_DW 1
#define SURFACE_MAX 2
// Connection
#define CONNECTED_NONE          0b000
#define CONNECTED_LEFT_TABLET   0b001
#define CONNECTED_RIGHT_TABLET  0b010
#define CONNECTED_CENTER_TABLET 0b100

typedef struct _Tablet
{
    gint ip;
    gint port;
} Tablet;

typedef struct _Surface {
    GstElement* video_sink;
    ANativeWindow* native_window;
} Surface;

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData
{
    jobject app;                  /* Application instance, used to call its methods. A global reference is kept. */
    GstElement *pipeline;         /* The running pipeline */
    GMainContext *context;        /* GLib context used to run the main loop */
    GMainLoop *main_loop;         /* GLib main loop */
    gboolean initialized;         /* To avoid informing the UI multiple times about the initialization */
    Surface surface[SURFACE_MAX]; /* Application Surfaces List */
    Tablet tablet[TABLET_ID_MAX];
    gint connections_status;
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

/* Change the ui state */
static void set_ui_state (GstState state, CustomData * data);

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
