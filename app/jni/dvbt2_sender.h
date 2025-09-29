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
#define PIPELINE_DESCRIPTION_NAMI_DVBT2_90 "ahcsrc ! " \
    /* Raise source framerate cap to 30fps (if camera supports it). */ \
    "video/x-raw,width=1280,height=720,framerate=30/1 ! " \
    "queue name=q max-size-buffers=6 max-size-bytes=0 max-size-time=0 ! " \
    "tee name=t " \
    /* DW preview branch: small, leaky queue keeps UI responsive */ \
    "t. ! queue name=tee1 max-size-buffers=2 max-size-bytes=0 max-size-time=0 ! " \
    "videoflip method=clockwise ! " \
    "glimagesink name=vsink1 " \
    /* FMMD preview branch: same idea */ \
    "t. ! queue name=tee2 max-size-buffers=2 max-size-bytes=0 max-size-time=0 ! " \
    "videoflip method=clockwise ! " \
    "glimagesink name=vsink2"

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

#endif //NAMIDTVBT2EXAMPLE_DVBT2_SENDER_H
