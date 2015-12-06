#include <jni.h>
#include <android/log.h>
#include <extractor/RtspExtractor.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#define LOG_TAG "NDK_RtspExtractor.cpp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void ffmpegInit(JNIEnv *env, jobject obj) {
	LOGI("%s", __FUNCTION__);
    avformat_network_init();
    av_register_all();
}

void ffmpegDeinit(JNIEnv *env, jobject obj) {
	LOGI("%s", __FUNCTION__);
    avformat_network_deinit();
}

jlong ffmpegOpenInput(JNIEnv *env, jobject obj, jstring juri) {
	LOGI("%s", __FUNCTION__);
    AVFormatContext *pFormatCtx = NULL;
    const char *uri = env->GetStringUTFChars(juri, 0);
    if (avformat_open_input(&pFormatCtx, uri, NULL, NULL) != 0) {
        LOGE("%s: Couldn't open %s", __FUNCTION__, uri);
        return -1;
    }

    env->ReleaseStringUTFChars(juri, uri);

    return (long) pFormatCtx;
}

void ffmpegCloseInput(JNIEnv *env, jobject obj, jlong formatCtx) {
	LOGI("%s", __FUNCTION__);
    AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;
    avformat_close_input(&pFormatCtx);
}

jobjectArray ffmpegGetTrackInfos(JNIEnv *env, jobject obj, jlong formatCtx) {
	LOGI("%s", __FUNCTION__);
    AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGE("%s: Couldn't find stream information", __FUNCTION__);
        return NULL;
    }

    jclass trackInfoClass = env->FindClass("me/courbiere/rtspsamplesource/TrackInfo");
    if (trackInfoClass == NULL) {
        LOGE("%s: Couldn't find TrackInfo class", __FUNCTION__);
        return NULL;
    }

    jobjectArray trackInfos = env->NewObjectArray(pFormatCtx->nb_streams, trackInfoClass, NULL);
    if (trackInfos == NULL) {
        LOGE("%s: Couldn't create TrackInfo array", __FUNCTION__);
        return NULL;
    }

    jmethodID trackInfoCtr = env->GetMethodID(trackInfoClass, "<init>", "(Ljava/lang/String;J)V");
    if (trackInfoCtr == NULL) {
        LOGE("%s: Couldn't get TrackInfo constructor", __FUNCTION__);
        return NULL;
    }

    long duration;
    AVCodecContext *pCodecCtx;

    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        duration = pFormatCtx->streams[i]->duration;
        pCodecCtx = pFormatCtx->streams[i]->codec;
        const char *mediaType = av_get_media_type_string(pCodecCtx->codec_type);
        const char *codecName = avcodec_get_name(pCodecCtx->codec_id);

        // HACK: set codecName appropriately for ExoPlayer because it's stupid
        const char *exoCodecName = (!strcmp(codecName, "h264") ? "avc" : codecName);

        char mime[strlen(mediaType) + strlen(exoCodecName) + 2];
        sprintf(mime, "%s/%s", mediaType, exoCodecName);
        jstring jmime = env->NewStringUTF(mime);

        jobject trackInfo = env->NewObject(trackInfoClass, trackInfoCtr, jmime, (jlong) duration);
        env->SetObjectArrayElement(trackInfos, i, trackInfo);
    }

    return trackInfos;
}

void ffmpegSelectTrack(JNIEnv *env, jobject obj, jlong formatCtx, jint index) {
	LOGI("%s", __FUNCTION__);
	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

	if (index >= pFormatCtx->nb_streams) {
		LOGE("%s: Index %d out of bounds - found %d streams", __FUNCTION__, index, pFormatCtx->nb_streams);
		return;
	}

	pFormatCtx->streams[index]->discard = AVDISCARD_NONE;
}

void ffmpegDeselectTrack(JNIEnv *env, jobject obj, jlong formatCtx, jint index) {

	LOGI("%s", __FUNCTION__);

	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

	if (index >= pFormatCtx->nb_streams) {
		LOGE("%s: Index %d out of bounds - found %d streams", __FUNCTION__, index, pFormatCtx->nb_streams);
		return;
	}

	pFormatCtx->streams[index]->discard = AVDISCARD_ALL;
}

void ffmpegSeekTo(JNIEnv *env, jobject obj, jlong formatCtx, jlong positionUs) {
	LOGI("%s", __FUNCTION__);
	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

	if (avformat_seek_file(pFormatCtx, -1, INT64_MIN, positionUs, INT64_MAX, 0) < 0) {
		LOGE("%s: Couldn't seek stream to %ld", __FUNCTION__, (long)positionUs);
	}
}

int ffmpegGetHeight(JNIEnv *env, jobject obj, jlong formatCtx, jint index) {
	LOGI("%s", __FUNCTION__);
	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

	if (index >= pFormatCtx->nb_streams) {
		LOGE("%s: Index %d out of bounds - found %d streams", __FUNCTION__, index, pFormatCtx->nb_streams);
		return -1;
	}
	AVStream *pStream = pFormatCtx->streams[index];
	if (!pStream || !pStream->codec) {
		LOGE("%s: NULL stream found for index %d", __FUNCTION__, index);
		return -1;
	}

	return pStream->codec->height;
}

int ffmpegGetWidth(JNIEnv *env, jobject obj, jlong formatCtx, jint index) {
	LOGI("%s", __FUNCTION__);
	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

	if (index >= pFormatCtx->nb_streams) {
		LOGE("%s: Index %d out of bounds - found %d streams", __FUNCTION__, index, pFormatCtx->nb_streams);
		return -1;
	}
	AVStream *pStream = pFormatCtx->streams[index];
	if (!pStream || !pStream->codec) {
		LOGE("%s: NULL stream found for index %d", __FUNCTION__, index);
		return -1;
	}

	return pStream->codec->width;
}

int ffmpegGetBitrate(JNIEnv *env, jobject obj, jlong formatCtx, jint index) {
	LOGI("%s", __FUNCTION__);
	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

	if (index >= pFormatCtx->nb_streams) {
		LOGE("%s: Index %d out of bounds - found %d streams", __FUNCTION__, index, pFormatCtx->nb_streams);
		return -1;
	}
	AVStream *pStream = pFormatCtx->streams[index];
	if (!pStream || !pStream->codec) {
		LOGE("%s: NULL stream found for index %d", __FUNCTION__, index);
		return -1;
	}

	return pStream->codec->bit_rate;
}

int ffmpegGetChannelCount(JNIEnv *env, jobject obj, jlong formatCtx, jint index) {
	LOGI("%s", __FUNCTION__);
	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

	if (index >= pFormatCtx->nb_streams) {
		LOGE("%s: Index %d out of bounds - found %d streams", __FUNCTION__, index, pFormatCtx->nb_streams);
		return -1;
	}
	AVStream *pStream = pFormatCtx->streams[index];
	if (!pStream || !pStream->codec) {
		LOGE("%s: NULL stream found for index %d", __FUNCTION__, index);
		return -1;
	}

	return pStream->codec->channels;
}

int ffmpegGetSampleRate(JNIEnv *env, jobject obj, jlong formatCtx, jint index) {
	LOGI("%s", __FUNCTION__);
	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

	if (index >= pFormatCtx->nb_streams) {
		LOGE("%s: Index %d out of bounds - found %d streams", __FUNCTION__, index, pFormatCtx->nb_streams);
		return -1;
	}
	AVStream *pStream = pFormatCtx->streams[index];
	if (!pStream || !pStream->codec) {
		LOGE("%s: NULL stream found for index %d", __FUNCTION__, index);
		return -1;
	}

	return pStream->codec->sample_rate;
}

jbyteArray ffmpegGetInitData(JNIEnv *env, jobject obj, jlong formatCtx, jint index) {

	LOGI("%s", __FUNCTION__);

	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

	if (index >= pFormatCtx->nb_streams) {
		LOGE("%s: Index %d out of bounds - found %d streams", __FUNCTION__, index, pFormatCtx->nb_streams);
		return NULL;
	}
	AVStream *pStream = pFormatCtx->streams[index];
	if (!pStream || !pStream->codec) {
		LOGE("%s: NULL stream found for index %d", __FUNCTION__, index);
		return NULL;
	}

	AVCodecContext *pCodecCtx = pStream->codec;
	jbyteArray ba = env->NewByteArray(pCodecCtx->extradata_size);
	jbyte *bytes = env->GetByteArrayElements(ba, 0);
	for (int i = 0; i < pCodecCtx->extradata_size; i++) {
		bytes[i] = pCodecCtx->extradata[i];
	}
	return ba;
}

void ffmpegPlay(JNIEnv *env, jobject obj, jlong formatCtx) {
	LOGI("%s", __FUNCTION__);
	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;

	// start playing
	LOGI("%s: Playing stream...", __FUNCTION__);
	if (av_read_play(pFormatCtx) < 0) {
		LOGE("%s: Couldn't play stream!", __FUNCTION__);
	}
	LOGI("%s: OK!", __FUNCTION__);
}

bool ffmpegReadSample(JNIEnv *env, jobject obj, jlong formatCtx, jobject sampleHolder) {
	//LOGI("%s", __FUNCTION__);
	AVFormatContext *pFormatCtx = (AVFormatContext*) formatCtx;
    AVPacket packet;
    av_init_packet(&packet);
    int ret = av_read_frame(pFormatCtx, &packet);
    if (ret < 0 || packet.size <= 0) {
    	av_free_packet(&packet);
    	return false;
    }

    // TODO: support one audio and one video track per stream
    AVMediaType type = pFormatCtx->streams[packet.stream_index]->codec->codec_type;
    //LOGI("%s: Frame Type: %d", __FUNCTION__, type);
    if (type != AVMEDIA_TYPE_VIDEO) {
    	av_free_packet(&packet);
    	return false;
    }

    // set flags for frame type
    jclass mediaExtractorClass = env->FindClass("android/media/MediaExtractor");
    jfieldID flagSyncFID = env->GetStaticFieldID(mediaExtractorClass, "SAMPLE_FLAG_SYNC", "I");
    int SAMPLE_FLAG_SYNC = env->GetStaticIntField(mediaExtractorClass, flagSyncFID);
    jclass sampleHolderClass = env->GetObjectClass(sampleHolder);
    jfieldID flagsFID = env->GetFieldID(sampleHolderClass, "flags", "I");
    jint flags = 0;
	if ((packet.flags & AV_PKT_FLAG_KEY) != 0) {
		flags |= SAMPLE_FLAG_SYNC;
	}
	env->SetIntField(sampleHolder, flagsFID, flags);

	// Get ByteBuffer object
    jclass byteBufferClass = env->FindClass("java/nio/ByteBuffer");
    jfieldID dataFID = env->GetFieldID(sampleHolderClass, "data", "Ljava/nio/ByteBuffer;");
    jobject dataObj = env->GetObjectField(sampleHolder, dataFID);

	// Get ByteBuffer position
    jmethodID positionMID = env->GetMethodID(byteBufferClass, "position", "()I");
    int offset = (int) env->CallIntMethod(dataObj, positionMID);
    //LOGI("%s: offset: %d", __FUNCTION__, offset);

    // Get the ByteBuffer's underlying byte array.
    jbyte *dst = (jbyte*) env->GetDirectBufferAddress(dataObj);
    if (dst == NULL) {
        LOGI("%s: dst is null", __FUNCTION__);
        av_free_packet(&packet);
        return false;
    }

    // Check size and offset
    size_t dstSize = (size_t) env->GetDirectBufferCapacity(dataObj);
    if (dstSize < offset) {
        LOGE("%s: ByteBuffer capacity (%d) less than offset (%d)", __FUNCTION__, dstSize, offset);
        av_free_packet(&packet);
        return false;
    }
    if (dstSize + offset < packet.size) {
        LOGE("%s: ByteBuffer capacity less than packet size", __FUNCTION__);
        av_free_packet(&packet);
        return false;
    }

    // Copy data to byte array
    dst += offset;
    memcpy((uint8_t*)dst, packet.data, packet.size);

    // Set new buffer limit and position
    positionMID = env->GetMethodID(byteBufferClass, "position", "(I)Ljava/nio/Buffer;");
    jmethodID limitMID = env->GetMethodID(byteBufferClass, "limit", "(I)Ljava/nio/Buffer;");
    jobject me = env->CallObjectMethod(dataObj, limitMID, offset + packet.size);
    env->DeleteLocalRef(me);
    me = env->CallObjectMethod(dataObj, positionMID, offset + packet.size);
    env->DeleteLocalRef(me);
    me = NULL;

	// set read size
	jfieldID sizeFID = env->GetFieldID(sampleHolderClass, "size", "I");
	env->SetIntField(sampleHolder, sizeFID, packet.size);

	// set timestamp
	double pts = packet.pts * av_q2d(pFormatCtx->streams[packet.stream_index]->time_base);
	//LOGI("%s: pts: %ld, newpts: %f, duration: %d, time_base: %d/%d", __FUNCTION__, packet.pts, pts, packet.duration, pFormatCtx->streams[packet.stream_index]->time_base.num, pFormatCtx->streams[packet.stream_index]->time_base.den);
	jfieldID timeFID = env->GetFieldID(sampleHolderClass, "timeUs", "J");
	env->SetLongField(sampleHolder, timeFID, long(pts * 1000000L));

	// clean up
	av_free_packet(&packet);

    return true;
}

int ffmpegTest(JNIEnv *env, jobject obj, jstring juri) {
	LOGI("%s", __FUNCTION__);
    AVFormatContext* context = avformat_alloc_context();
    int video_stream_index;

    av_register_all();
    avformat_network_init();

    //open rtsp
    const char *uri = env->GetStringUTFChars(juri, 0);
    if(avformat_open_input(&context, uri, NULL, NULL) != 0){
    	LOGE("%s: Couldn't open input", __FUNCTION__);
        return EXIT_FAILURE;
    }
    env->ReleaseStringUTFChars(juri, uri);

    if(avformat_find_stream_info(context,NULL) < 0){
    	LOGE("%s: Couldn't find stream info", __FUNCTION__);
        return EXIT_FAILURE;
    }

    //search video stream
    for(int i =0;i<context->nb_streams;i++){
        if(context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            video_stream_index = i;
    }

    AVPacket packet;
    av_init_packet(&packet);

    AVStream* stream=NULL;
    int cnt = 0;

	LOGI("%s: Starting playing the stream (long name: %s, read_play func: %p)", __FUNCTION__, context->iformat->long_name, context->iformat->read_play);
    //av_read_play(context);//play RTSP

    LOGI("%s: OK: Reading frames...", __FUNCTION__);
    while(av_read_frame(context,&packet)>=0 && cnt <100){//read 100 frames
        if(packet.stream_index == video_stream_index){//packet is video
        	LOGI("%s: Read %d bytes", __FUNCTION__, packet.size);
        }
        av_free_packet(&packet);
        av_init_packet(&packet);
    }

    avformat_close_input(&context);
    avformat_network_deinit();

    LOGI("%s: Donesky", __FUNCTION__);

    return EXIT_SUCCESS;
}

const char *kRtspExtractorClassPath = "me/courbiere/rtspsamplesource/RtspSampleSource$RtspSampleSourceReader";
static JNINativeMethod methodTable[] = {
        {"ffInit",       "()V",                   (void *) ffmpegInit},
        {"ffDeinit",     "()V",                   (void *) ffmpegDeinit},
        {"ffOpenInput",  "(Ljava/lang/String;)J", (void *) ffmpegOpenInput},
        {"ffCloseInput", "(J)V",                  (void *) ffmpegCloseInput},
        {"ffGetTrackInfos", "(J)[me/courbiere/rtspsamplesource/TrackInfo;", (void*) ffmpegGetTrackInfos},
		{"ffSelectTrack", "(JI)V", (void*) ffmpegSelectTrack},
		{"ffDeselectTrack", "(JI)V", (void*) ffmpegDeselectTrack},
        {"ffSeekTo", "(JJ)V", (void*) ffmpegSeekTo},
		{"ffGetHeight", "(JI)I", (void*) ffmpegGetHeight},
		{"ffGetWidth", "(JI)I", (void*) ffmpegGetWidth},
		{"ffGetBitrate", "(JI)I", (void*) ffmpegGetBitrate},
		{"ffGetChannelCount", "(JI)I", (void*) ffmpegGetChannelCount},
		{"ffGetSampleRate", "(JI)I", (void*) ffmpegGetSampleRate},
		{"ffGetInitData", "(JI)[B", (void*) ffmpegGetInitData},
		{"ffPlay", "(J)V", (void*) ffmpegPlay},
		{"ffTest", "(Ljava/lang/String;)I", (void*) ffmpegTest},
		{"ffReadSample", "(JLcom/google/android/exoplayer/SampleHolder;)Z", (void*) ffmpegReadSample}
};

int RtspExtractor_registerNativeMethods(JNIEnv *env) {
    jclass rtspExtractorClass = env->FindClass(kRtspExtractorClassPath);
    if (!rtspExtractorClass) {
        LOGE("JNI_OnLoad(): Failed to get %s class reference", kRtspExtractorClassPath);
        return -1;
    }

    return env->RegisterNatives(rtspExtractorClass, methodTable,
                                sizeof(methodTable) / sizeof(methodTable[0]));
}
