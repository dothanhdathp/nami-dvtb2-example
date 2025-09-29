package nami.example.dtvbt2

import android.app.Activity
import android.util.Log
import android.view.SurfaceHolder
import android.view.View
import android.widget.TextView

class DvbSenderManager(private val callback: DvbSenderManagerCallback) {
    private val TAG = "DvbSenderManager"

    private external fun nativeInit() // Initialize native code, build pipeline, etc
    private external fun nativeFinalize() // Destroy pipeline and shutdown native code
    private external fun nativePlay() // Set pipeline to PLAYING
    private external fun nativePause() // Set pipeline to PAUSED
    private external fun nativeSurfaceInit(id: Int, surface: Any)
    private external fun nativeSurfaceFinalize(id: Int)
    private val nativeCustomData: Long = 0 // Native code will use this to keep private data
    private var dvbt_ready = false // Whether the user asked to go to PLAYING

    fun init() {
        if(!dvbt_ready)
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
        // Phần này chắc là quyết định đóng hoàn toàn streamm hay không là do thằng này quyết định.
        nativeFinalize()
    }

    fun handlePlay() {
        if(dvbt_ready) {
            nativePlay()
        } else {
            Log.d(TAG, "handlePlay: Wait module ready!")
        }
    }

    fun handlePause() {
        if(dvbt_ready) {
            nativePause()
        } else {
            Log.d(TAG, "handlePause: Wait module ready!")
        }
    }

    /* Native Call Back
     * Called from native code. This sets the content of the TextView from the UI thread.
    */
    private fun setMessage(message: String) {
        Log.i(TAG, "Got messaged $message")
        callback.onCallBack(DvbSenderManagerCallback.dvbt_event.DVBT_COMMON_MESSAGE, message)
    }

    // the main loop is running, so it is ready to accept commands.
    private fun onGStreamerInitialized() {
        Log.i(TAG, "Gst initialized. Restoring state, playing: ???")
        // Re-enable buttons, now that GStreamer is initialized
        dvbt_ready = true;
        callback.onCallBack(DvbSenderManagerCallback.dvbt_event.DVBT_INITIALIZED)
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