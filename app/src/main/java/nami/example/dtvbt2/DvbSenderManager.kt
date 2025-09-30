package nami.example.dtvbt2

import android.app.Activity
import android.util.Log
import android.view.SurfaceHolder
import android.view.View
import android.widget.TextView

class DvbSenderManager(private val callback: DvbSenderManagerCallback) {
    private val TAG = "DvbSenderManager"
    private var mGstState: Int = -1

    private external fun nativeInit() // Initialize native code, build pipeline, etc
    private external fun nativeFinalize() // Destroy pipeline and shutdown native code
    private external fun nativePlay() // Set pipeline to PLAYING
    private external fun nativePause() // Set pipeline to PAUSED
    private external fun nativeSurfaceInit(id: Int, surface: Any)
    private external fun nativeSurfaceFinalize(id: Int)
    private val nativeCustomData: Long = 0 // Native code will use this to keep private data

    fun init() {
        nativeInit()
    }

    // Directly call form UI
    fun setSurface(id: Int, holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d("GStreamer", "Surface $id changed to format $format width $width height $height")
        nativeSurfaceInit(id, holder.surface)
    }

    fun surfaceFinalize(id: Int) {
        nativeSurfaceFinalize(id)
    }

    fun finalize() {
        nativeFinalize()
    }

    fun handlePlay() {
            nativePlay()
    }

    fun handlePause() {
        nativePause()
    }

    /* Native Call Back
     * Called from native code. This sets the content of the TextView from the UI thread.
    */
    private fun setMessage(message: String) {
        Log.i(TAG, "Got messaged $message")
        callback.handleDvbEvent(DvbSenderManagerCallback.dvbt_event.DVBT_COMMON_MESSAGE, message)
    }

    // the main loop is running, so it is ready to accept commands.
    private fun onGStreamerInitialized() {
        Log.i(TAG, "Gst initialized. Restoring state, playing: ???")
        callback.handleDvbEvent(DvbSenderManagerCallback.dvbt_event.DVBT_INITIALIZED)
    }

    /**
     * Defined enum value check in "gstelement.h", which corresponding to GStreamer state
     * - GST_STATE_VOID_PENDING        = 0,
     * - GST_STATE_NULL                = 1,
     * - GST_STATE_READY               = 2,
     * - GST_STATE_PAUSED              = 3,
     * - GST_STATE_PLAYING             = 4
        * - GST_STATE_UNKNOW           = -1 (not set or unknown)
     */
    private fun onGStreamerState(state: Int) {
        // Convert to String state
        var str_state: String = "GST_STATE_NULL"
        when (state) {
            0 -> str_state = "GST_STATE_VOID_PENDING"
            1 -> str_state = "GST_STATE_NULL"
            2 -> str_state = "GST_STATE_READY"
            3 -> str_state = "GST_STATE_PAUSED"
            4 -> str_state = "GST_STATE_PLAYING"
        }
        Log.i(TAG, "Gst state changed: $str_state")
    }

    companion object {
        @JvmStatic
        private external fun nativeClassInit(): Boolean // Initialize native class: cache Method IDs for callbacks

        init {
            System.loadLibrary("gstreamer_android")
            System.loadLibrary("dvbt2_sender")
            nativeClassInit()
        }
    }
}