package nami.example.dtvbt2;


interface DvbSenderManagerCallback {
    enum class dvbt_event {
        DVBT_INITIALIZED,
        DVBT_TERMINATED,
        DVBT_COMMON_MESSAGE,
    }
    fun onCallBack(e: dvbt_event, message: String? = null)
}