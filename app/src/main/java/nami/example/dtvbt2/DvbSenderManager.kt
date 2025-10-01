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
    private var mCameraEnabled: Boolean = false
    private var mDabInited: Boolean = false
    private var mDabState: Int = DvbSenderManagerCallback.GST_STATE_VOID_PENDING

    // Set the preset condition value
    fun setCamera(value: Boolean) {
        mCameraEnabled = value
    }

    fun init() {
        if(!mCameraEnabled) {
            Log.e(TAG, "init: Camera not allow to access!")
            return
        }
        Log.e(TAG, "init: Init Suggest!")
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

    fun callPlay() {
        nativePlay()
    }

    fun callPause() {
        nativePause()
    }

    /* Native Call Back
     * Called from native code. This sets the content of the TextView from the UI thread.
    */
    private fun setMessage(message: String) {
        callback.handleDvbEvent(DvbSenderManagerCallback.dvbt_event.DVBT_COMMON_MESSAGE, null, message)
    }

    // the main loop is running, so it is ready to accept commands.
    private fun onGStreamerInitialized() {
        mDabInited = true
        callback.handleDvbEvent(DvbSenderManagerCallback.dvbt_event.DVBT_INITIALIZED, null, null)
    }

    private fun onGStreamerState(state: Int) {
        // Convert to String state
        mDabState = state
        Log.i(TAG, "onGStreamerState: $state - " + DvbSenderManagerCallback.gstStateToString(state))
        callback.handleDvbEvent(DvbSenderManagerCallback.dvbt_event.DVBT_ON_STATE, state, null)
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