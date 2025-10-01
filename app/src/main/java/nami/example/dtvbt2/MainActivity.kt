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

class MainActivity : Activity(), DvbSenderManagerCallback {
    private var isPlayingDesired = false // Whether the user asked to go to PLAYING
    private val TAG: String = "MainActivity" // Whether the user asked to go to PLAYING"
    private val SURFACE_FMMW: Int = 0 // Native, always exist
    private val SURFACE_DW: Int = 1
    private var allowed_camera: Boolean = false
    private val CAMERA_PERMISSION_CODE = 100
    private val dvbSenderManager: DvbSenderManager = DvbSenderManager(this)
    private var mBtnPlay: ImageButton? = null;
    private var mBtnPause: ImageButton? = null;

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
        // On camera allow access start dvbSenderManager
        dvbSenderManager.setCamera(true)
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
        mBtnPlay = findViewById<View>(R.id.button_play) as ImageButton
        mBtnPause = findViewById<View>(R.id.button_stop) as ImageButton
        mBtnPlay?.setOnClickListener {
            dvbSenderManager.callPlay()
        }
        mBtnPause?.setOnClickListener {
            dvbSenderManager.callPause()
        }

        val svf = findViewById<View>(R.id.surface_fmmd) as SurfaceView
        svf.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
            }

            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                // Handle size or format changes if needed
                dvbSenderManager.setSurface(SURFACE_FMMW, holder, format, width, height)
            }

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                dvbSenderManager.surfaceFinalize(SURFACE_FMMW)
            }
        })
        val svd = findViewById<View>(R.id.surface_dw) as SurfaceView
        svd.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
            }

            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                // Handle size or format changes if needed
                dvbSenderManager.setSurface(SURFACE_DW, holder, format, width, height)
            }

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                dvbSenderManager.surfaceFinalize(SURFACE_DW)
            }
        })

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

    override fun handleDvbEvent(
        e: DvbSenderManagerCallback.dvbt_event,
        value: Int?,
        message: String?
    ) {
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
                DvbSenderManagerCallback.dvbt_event.DVBT_ON_STATE -> {
                    when(value) {
                        DvbSenderManagerCallback.GST_STATE_VOID_PENDING -> {
                            // to do
                        }
                        DvbSenderManagerCallback.GST_STATE_NULL -> {
                            // to do
                        }
                        DvbSenderManagerCallback.GST_STATE_READY -> {
                            dvbSenderManager.callPlay();
                        }
                        DvbSenderManagerCallback.GST_STATE_PAUSED -> {
                            // to do
                        }
                        DvbSenderManagerCallback.GST_STATE_PLAYING -> {
                            // to do
                        }
                    }
                }
                else -> {
                   Log.i(TAG, "onCallBack (Unknown): ${e.toString()}")
                }
            }
        }
    }
}