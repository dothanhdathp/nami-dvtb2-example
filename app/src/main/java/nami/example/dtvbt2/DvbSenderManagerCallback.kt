package nami.example.dtvbt2;


interface DvbSenderManagerCallback {
    enum class dvbt_event {
        DVBT_INITIALIZED,
        DVBT_TERMINATED,
        DVBT_COMMON_MESSAGE,
        DVBT_ON_STATE,
    }
    /**
     * Defined enum value check in "gstelement.h", which follow GStreamer state
     * - GST_STATE_VOID_PENDING        = 0,
     * - GST_STATE_NULL                = 1,
     * - GST_STATE_READY               = 2,
     * - GST_STATE_PAUSED              = 3,
     * - GST_STATE_PLAYING             = 4
     */
    companion object {
        const val GST_STATE_VOID_PENDING = 0
        const val GST_STATE_NULL         = 1
        const val GST_STATE_READY        = 2
        const val GST_STATE_PAUSED       = 3
        const val GST_STATE_PLAYING      = 4

        fun gstStateToString(state: Int): String {
            when(state) {
                GST_STATE_VOID_PENDING -> return "GST_STATE_VOID_PENDING"
                GST_STATE_NULL -> return "GST_STATE_NULL"
                GST_STATE_READY -> return "GST_STATE_READY"
                GST_STATE_PAUSED -> return "GST_STATE_PAUSED"
                GST_STATE_PLAYING -> return "GST_STATE_PLAYING"
                else -> return "<ERROR_UNKNOWN_STATE>"
            }
        }
    }

    fun handleDvbEvent(e: dvbt_event, value: Int? = null, message: String? = null)
}