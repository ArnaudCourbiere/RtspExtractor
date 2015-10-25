#include <jni.h>
#include <android/log.h>
#include <extractor/RtspExtractor.h>

#define LOG_TAG "NDK_FFMPEGTest.cpp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("JNI_OnLoad(): Failed to get the environment");
        return -1;
    }

    if (RtspExtractor_registerNativeMethods(env) < 0) {
        LOGE("JNI_OnLoad(): RtspExtractor native registration failed");
        return -1;
    }

    return JNI_VERSION_1_6;
}
