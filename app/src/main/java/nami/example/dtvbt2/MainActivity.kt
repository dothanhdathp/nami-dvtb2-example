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
import nami.example.dtvbt2.DvbSenderManager

class MainActivity : Activity(), DvbSenderManagerCallback {
    private var isPlayingDesired = false // Whether the user asked to go to PLAYING
    private val TAG: String = "MainActivity" // Whether the user asked to go to PLAYING"
    private val SURFACE_FMMW: Int = 0 // Native, always exist
    private val SURFACE_DW: Int = 1

    // Callback for SurfaceView 1
    val srfcb_fmmd = object : SurfaceHolder.Callback {
        override fun surfaceCreated(holder: SurfaceHolder) {
            Log.d("SurfaceView1", "Surface created")
            // Initialize rendering or GStreamer pipeline here
        }

        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            Log.d("SurfaceView1", "Surface changed")
            dvbSenderManager.setSurface(SURFACE_FMMW, holder, format, width, height)
        }

        override fun surfaceDestroyed(holder: SurfaceHolder) {
            Log.d("SurfaceView1", "Surface destroyed")
            dvbSenderManager.surfaceFinalize(SURFACE_FMMW)
        }
    }

    // Callback for SurfaceView 2
    val srfcb_dw = object : SurfaceHolder.Callback {
        override fun surfaceCreated(holder: SurfaceHolder) {
            Log.d("SurfaceView2", "Surface created")
            // Initialize second rendering or GStreamer pipeline here
        }

        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            Log.d("SurfaceView2", "Surface changed")
            dvbSenderManager.setSurface(SURFACE_DW, holder, format, width, height)
        }

        override fun surfaceDestroyed(holder: SurfaceHolder) {
            Log.d("SurfaceView2", "Surface destroyed")
            dvbSenderManager.surfaceFinalize(SURFACE_DW)
        }
    }

    // Check Camera Permission
    private val CAMERA_PERMISSION_CODE = 100
    private val dvbSenderManager: DvbSenderManager = DvbSenderManager(this)

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
        dvbSenderManager.init();
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
        val play = findViewById<View>(R.id.button_play) as ImageButton
        play.setOnClickListener {
            dvbSenderManager.handlePlay()
        }
        val pause = findViewById<View>(R.id.button_stop) as ImageButton
        pause.setOnClickListener {
            isPlayingDesired = false
            dvbSenderManager.handlePause()
        }

        val sv_dw = findViewById<View>(R.id.surface_dw) as SurfaceView
        val sv_fmmd = findViewById<View>(R.id.surface_fmmd) as SurfaceView
        sv_dw.holder.addCallback(srfcb_dw)
        sv_fmmd.holder.addCallback(srfcb_fmmd)

        if (savedInstanceState != null) {
            isPlayingDesired = savedInstanceState.getBoolean("playing")
            Log.i(TAG, "Activity created. Saved state is playing:$isPlayingDesired")
        } else {
            isPlayingDesired = false
            Log.i(TAG, "Activity created. There is no saved state, playing: false")
        }

        // Start with disabled buttons, until native code is initialized
        findViewById<View>(R.id.button_play).isEnabled = false
        findViewById<View>(R.id.button_stop).isEnabled = false
    }

    override fun onSaveInstanceState(outState: Bundle) {
        Log.d(TAG, "Saving state, playing:$isPlayingDesired")
        outState.putBoolean("playing", isPlayingDesired)
    }

    override fun onDestroy() {
        dvbSenderManager.finalize()
        super.onDestroy()
    }

//    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
//        dvbSenderManager.setSurface(holder, format, width, height)
//    }
//
//    override fun surfaceCreated(holder: SurfaceHolder) {
//        Log.d(TAG, "Surface created: " + holder.surface)
//        // Then ???
//    }
//
//    override fun surfaceDestroyed(holder: SurfaceHolder) {
//        Log.d(TAG, "Surface destroyed")
//        dvbSenderManager.surfaceFinalize()
//    }

    // Callback from DvbSenderManager
    override fun setMessage(message: String) {
        val tv = findViewById<View>(R.id.textview_message) as TextView
        runOnUiThread { tv.text = message }
    }

    override fun dvbtInitialized() {
        // Enable button
        Log.d(TAG, "dvbtInitialized")
        findViewById<View>(R.id.button_play).isEnabled = true
        findViewById<View>(R.id.button_stop).isEnabled = true
        dvbSenderManager.handlePlay()
    }

    override fun dvbtTerminated() {
        // Disable button
        findViewById<View>(R.id.button_play).isEnabled = false
        findViewById<View>(R.id.button_stop).isEnabled = false
    }
}