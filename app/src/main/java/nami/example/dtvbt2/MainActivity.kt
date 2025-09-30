package nami.example.dtvbt2

import android.app.Activity
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.ImageButton
import android.widget.TextView
import android.widget.Toast
import android.Manifest
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import org.freedesktop.gstreamer.GStreamer
import nami.example.dtvbt2.DvbSenderManagerCallback

class MainActivity : Activity(), SurfaceHolder.Callback, DvbSenderManagerCallback {
    private var isPlayingDesired = false // Whether the user asked to go to PLAYING
    private val TAG: String = "MainActivity" // Whether the user asked to go to PLAYING"
    private val SURFACE_FMMW: Int = 0 // Native, always exist
    private val SURFACE_DW: Int = 1
    private var allowed_camera: Boolean = false
    private val CAMERA_PERMISSION_CODE = 100
    private val dvbSenderManager: DvbSenderManager = DvbSenderManager(this)
    private var mBtnPlay: ImageButton? = null;
    private var mBtnPause: ImageButton? = null;

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.d("SurfaceView1", "Surface created")
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d("SurfaceView1", "Surface changed")
        dvbSenderManager.setSurface(SURFACE_FMMW, holder, format, width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d("SurfaceView1", "Surface destroyed")
        dvbSenderManager.surfaceFinalize(SURFACE_FMMW)
    }

    // Check Camera Permission

    private fun checkCameraPermission() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {

            ActivityCompat.requestPermissions(
                this,
                arrayOf(Manifest.permission.CAMERA),
                CAMERA_PERMISSION_CODE
            )
        } else {
            // Permission already granted
            startCamera()
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == CAMERA_PERMISSION_CODE) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                startCamera()
            } else {
                Toast.makeText(this, "Camera permission denied", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun startCamera() {
        // Your camera logic here
        allowed_camera = true
    }

    //*-----------------------------------------------------------------------------------------------------------------------------------------------------------------*/
    // Called when the activity is first created.
    public override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Initialize GStreamer and warn if it fails
        try {
            GStreamer.init(this)
        } catch (e: Exception) {
            Toast.makeText(this, e.message, Toast.LENGTH_LONG).show()
            finish()
            return
        }

        checkCameraPermission()

        setContentView(R.layout.activity_main)
        mBtnPlay = findViewById<View>(R.id.button_play) as ImageButton
        mBtnPause = findViewById<View>(R.id.button_stop) as ImageButton
        mBtnPlay?.setOnClickListener {
            dvbSenderManager.handlePlay()
        }
        mBtnPause?.setOnClickListener {
            dvbSenderManager.handlePause()
        }

        val sv = findViewById<View>(R.id.surface_fmmd) as SurfaceView
        sv.holder.addCallback(this)

        if (savedInstanceState != null) {
            isPlayingDesired = savedInstanceState.getBoolean("playing")
            Log.i(TAG, "Activity created. Saved state is playing:$isPlayingDesired")
        } else {
            isPlayingDesired = false
            Log.i(TAG, "Activity created. There is no saved state, playing: false")
        }

        // Start with disabled buttons, until native code is initialized
        mBtnPlay?.isEnabled = false
        mBtnPause?.isEnabled = false

        if(allowed_camera)
            dvbSenderManager.init();
    }

    override fun onSaveInstanceState(outState: Bundle) {
        Log.d(TAG, "Saving state, playing:$isPlayingDesired")
        outState.putBoolean("playing", isPlayingDesired)
    }

    override fun onDestroy() {
        dvbSenderManager.finalize()
        super.onDestroy()
    }

    override fun handleDvbEvent(e: DvbSenderManagerCallback.dvbt_event, message: String?) {
        runOnUiThread {
            when (e) {
                 DvbSenderManagerCallback.dvbt_event.DVBT_INITIALIZED -> {
                     Log.i(TAG, "onCallBack: ${e.toString()}")
                     mBtnPlay?.isEnabled = true
                     mBtnPause?.isEnabled = true
                 }
                 DvbSenderManagerCallback.dvbt_event.DVBT_TERMINATED -> {
                     Log.i(TAG, "onCallBack: ${e.toString()}")
                 }
                 DvbSenderManagerCallback.dvbt_event.DVBT_COMMON_MESSAGE -> {
                     val tv = findViewById<View>(R.id.textview_message) as TextView
                     tv.text = message
                 }
                 else -> {
                    Log.i(TAG, "onCallBack (Unknown): ${e.toString()}")
                 }
            }
        }
    }
}