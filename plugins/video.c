#include "sdlvideo.h"

void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    AVPacketList *pkt1;

    pkt1 = (AVPacketList *) av_malloc(sizeof(AVPacketList));
    if (!pkt1) {
        return -1;
    }
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex);

    if (!q->last_pkt) {
        q->first_pkt = pkt1;
    } else {
        q->last_pkt->next = pkt1;
    }

    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);
    SDL_UnlockMutex(q->mutex);
    return 0;
}

int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {
        if (is->quit) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt) {
                q->last_pkt = NULL;
            }
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;

            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }

    SDL_UnlockMutex(q->mutex);

    return ret;
}

double get_audio_clock(VideoState *is) {
    double pts;
    int hw_buf_size, bytes_per_sec, n;

    pts = is->audio_clock; /* maintained in the audio thread */
    hw_buf_size = is->audio_buf_size - is->audio_buf_index;
    bytes_per_sec = 0;
    n = is->audio_st->codec->channels * 2;
    if (is->audio_st) {
        bytes_per_sec = is->audio_st->codec->sample_rate * n;
    }
    if (bytes_per_sec) {
        pts -= (double) hw_buf_size / bytes_per_sec;
    }
    return pts;
}

int audio_decode_frame(VideoState *is, double *pts_ptr) {
    int len1, len2, decoded_data_size;
    AVPacket *pkt = &is->audio_pkt;
    int got_frame = 0;
    int64_t dec_channel_layout;
    int wanted_nb_samples, resampled_data_size, n;

    double pts;

    for (;;) {
        while (is->audio_pkt_size > 0) {
            if (!is->audio_frame) {
                if (!(is->audio_frame = avcodec_alloc_frame())) {
                    return AVERROR(ENOMEM);
                }
            } else
                avcodec_get_frame_defaults(is->audio_frame);

            len1 = avcodec_decode_audio4(is->audio_st->codec, is->audio_frame,
                    &got_frame, pkt);
            if (len1 < 0) {
                // error, skip the frame
                is->audio_pkt_size = 0;
                break;
            }

            is->audio_pkt_data += len1;
            is->audio_pkt_size -= len1;

            if (!got_frame)
                continue;

            decoded_data_size = av_samples_get_buffer_size(NULL,
                    is->audio_frame->channels, is->audio_frame->nb_samples,
                    is->audio_frame->format, 1);

            dec_channel_layout =
                    (is->audio_frame->channel_layout
                            && is->audio_frame->channels
                                    == av_get_channel_layout_nb_channels(
                                            is->audio_frame->channel_layout)) ?
                            is->audio_frame->channel_layout :
                            av_get_default_channel_layout(
                                    is->audio_frame->channels);

            wanted_nb_samples = is->audio_frame->nb_samples;

            if (is->audio_frame->format != is->audio_src_fmt
                    || dec_channel_layout != is->audio_src_channel_layout
                    || is->audio_frame->sample_rate != is->audio_src_freq
                    || (wanted_nb_samples != is->audio_frame->nb_samples
                            && !is->swr_ctx)) {
                if (is->swr_ctx)
                    swr_free(&is->swr_ctx);
                is->swr_ctx = swr_alloc_set_opts(NULL,
                        is->audio_tgt_channel_layout, is->audio_tgt_fmt,
                        is->audio_tgt_freq, dec_channel_layout,
                        is->audio_frame->format, is->audio_frame->sample_rate,
                        0, NULL);
                if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
                    slog(LVL_QUIET,ERROR, "swr_init() failed");
                    break;
                }
                is->audio_src_channel_layout = dec_channel_layout;
                is->audio_src_channels = is->audio_st->codec->channels;
                is->audio_src_freq = is->audio_st->codec->sample_rate;
                is->audio_src_fmt = is->audio_st->codec->sample_fmt;
            }

            if (is->swr_ctx) {
                const uint8_t **in =
                        (const uint8_t **) is->audio_frame->extended_data;
                uint8_t *out[] = { is->audio_buf2 };
                if (wanted_nb_samples != is->audio_frame->nb_samples) {
                    if (swr_set_compensation(is->swr_ctx,
                            (wanted_nb_samples - is->audio_frame->nb_samples)
                                    * is->audio_tgt_freq
                                    / is->audio_frame->sample_rate,
                            wanted_nb_samples * is->audio_tgt_freq
                                    / is->audio_frame->sample_rate) < 0) {
                        slog(LVL_QUIET,ERROR, "swr_set_compensation() failed");
                        break;
                    }
                }

                len2 = swr_convert(is->swr_ctx, out,
                        sizeof(is->audio_buf2) / is->audio_tgt_channels
                                / av_get_bytes_per_sample(is->audio_tgt_fmt),
                        in, is->audio_frame->nb_samples);
                if (len2 < 0) {
                    slog(LVL_QUIET,ERROR, "swr_convert() failed");
                    break;
                }
                if (len2
                        == sizeof(is->audio_buf2) / is->audio_tgt_channels
                                / av_get_bytes_per_sample(is->audio_tgt_fmt)) {
                    slog(LVL_INFO,WARN,"warning: audio buffer is probably too small");
                    swr_init(is->swr_ctx);
                }
                is->audio_buf = is->audio_buf2;
                resampled_data_size = len2 * is->audio_tgt_channels
                        * av_get_bytes_per_sample(is->audio_tgt_fmt);
            } else {
                resampled_data_size = decoded_data_size;
                is->audio_buf = is->audio_frame->data[0];
            }

            pts = is->audio_clock;
            *pts_ptr = pts;
            n = 2 * is->audio_st->codec->channels;
            is->audio_clock += (double) resampled_data_size
                    / (double) (n * is->audio_st->codec->sample_rate);

            // We have data, return it and come back for more later
            return resampled_data_size;
        }

        if (pkt->data)
            av_free_packet(pkt);
        memset(pkt, 0, sizeof(*pkt));
        if (is->quit)
            return -1;
        if (packet_queue_get(&is->audioq, pkt, 1) < 0)
            return -1;

        is->audio_pkt_data = pkt->data;
        is->audio_pkt_size = pkt->size;

        /* if update, update the audio clock w/pts */
        if (pkt->pts != AV_NOPTS_VALUE) {
            is->audio_clock = av_q2d(is->audio_st->time_base) * pkt->pts;
        }
    }

    return 0;
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
    VideoState *is = (VideoState *) userdata;
    int len1, audio_data_size;

    double pts;

    while (len > 0) {

        if (is->audio_buf_index >= is->audio_buf_size) {
            audio_data_size = audio_decode_frame(is, &pts);
            if (audio_data_size < 0) {
                /* silence */
                is->audio_buf_size = 1024;
                memset(is->audio_buf, 0, is->audio_buf_size);
            } else {
                is->audio_buf_size = audio_data_size;
            }
            is->audio_buf_index = 0;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len) {
            len1 = len;
        }

        memcpy(stream, (uint8_t *) is->audio_buf + is->audio_buf_index, len1);
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
}

static Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque) 
{
    int i;
    if(is->quit == false && ph->playVideo == true){
        //callNonBlocking("video::video_refresh_timer", &i, opaque);
        SDL_Event event;
        event.type = SDL_USEREVENT;
        event.user.data1 = "video::video_refresh_timer";
        event.user.data2 = opaque;
        SDL_PushEvent(&event);
    }
    return 0;
}

void schedule_refresh(VideoState *is, int delay) {
        SDL_AddTimer(delay, sdl_refresh_timer_cb, is);
}

int decode_interrupt_cb(void *opaque) {
    return (is && is->quit);
}

void video_display(VideoState *is) {
    SDL_Rect srcRect, dstRect;
    VideoPicture *vp;
    float aspect_ratio;

    vp = &is->pictq[is->pictq_rindex];
    if (is->TI->texture) {
        if (is->video_st->codec->sample_aspect_ratio.num == 0) {
            aspect_ratio = 0;
        } else {
            //aspect_ratio = av_q2d(is->video_st->codec->sample_aspect_ratio)
            //        * is->video_st->codec->width / is->video_st->codec->height;
            aspect_ratio = av_q2d(is->video_st->codec->sample_aspect_ratio)
                    * vp->width / vp->height;
        }

        if (aspect_ratio <= 0.0) {
            aspect_ratio = (float) vp->width / (float) vp->height;
        }

        srcRect.x = 0;
        srcRect.y = 0;
        srcRect.w = vp->width;
        srcRect.h = vp->height;
        dstRect.x = is->videoPosX;
        dstRect.y = is->videoPosY;
        dstRect.w = is->TI->rect->w;
        dstRect.h = is->TI->rect->h;
        //dstRect.w = vp->width;
        //dstRect.h = vp->height;
        long crc=0;
        //if(ph->loglevel > FULLDEBUG){
        //    for(int i = 0; i < vp->width*vp->height; i++) {
        //        char *a = (char *)vp->rawdata->data[0];
        //        char *b = (char *)vp->rawdata->data[1];
        //        char *c = (char *)vp->rawdata->data[2];
        //        crc = crc + a[i] + b[i] + c[i];
        //    }
        //}
        slog(DEBUG,FULLDEBUG,"UpdateYUV {%d,%d,%d,%d} -> {%d,%d,%d,%d}, %d, %u",
                srcRect.x, srcRect.y, srcRect.w, srcRect.h,
                dstRect.x, dstRect.y, dstRect.w, dstRect.h, crc);
        SDL_UpdateYUVTexture(is->TI->texture, &srcRect, 
                vp->rawdata->data[0], vp->rawdata->linesize[0], 
                vp->rawdata->data[1], vp->rawdata->linesize[1], 
                vp->rawdata->data[2], vp->rawdata->linesize[2]);
        if(is->directRender){
            SDL_SetRenderTarget(ph->renderer,NULL);
            SDL_RenderClear(ph->renderer);
            //SDL_RenderCopy(ph->renderer, is->TI->texture, NULL,NULL);
            SDL_RenderCopy(ph->renderer, is->TI->texture, &srcRect, &dstRect);
            SDL_RenderPresent(ph->renderer);
        } else {
            renderActive(is->TI->name);
        }
    }
}

void video_refresh_timer(void *userdata) {
    VideoState *is = (VideoState *) userdata;
    VideoPicture *vp;
    double actual_delay, delay, sync_threshold, ref_clock, diff;
    if (is->video_st && !is->pause && !is->quit) {
        if (is->pictq_size == 0) {
            schedule_refresh(is, 1);
        } else {
            is->read_frame_fail=0;
            vp = &is->pictq[is->pictq_rindex];

            delay = vp->pts - is->frame_last_pts; /* the pts from last time */
            if (delay <= 0 || delay >= 1.0) {
                /* if incorrect delay, use previous one */
                delay = is->frame_last_delay;
            }
            /* save for next time */
            is->frame_last_delay = delay;
            is->frame_last_pts = vp->pts;

	    if(is->hasAudio == true){
            	/* update delay to sync to audio */
            	ref_clock = get_audio_clock(is);
            	diff = vp->pts - ref_clock;
	    } else {
		diff = 0;
	    }

            /* Skip or repeat the frame. Take delay into account
             FFPlay still doesn't "know if this is the best guess." */
            sync_threshold =
                    (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;
            if (fabs(diff) < AV_NOSYNC_THRESHOLD) {
                if (diff <= -sync_threshold) {
                    delay = 0;
                } else if (diff >= sync_threshold) {
                    delay = 2 * delay;
                }
            }
            is->frame_timer += delay;
            /* computer the REAL delay */
            actual_delay = is->frame_timer - (av_gettime() / 1000000.0);
            if (actual_delay < 0.010) {
                /* Really it should skip the picture instead */
                actual_delay = 0.010;
            }
            schedule_refresh(is, (int) (actual_delay * 1000 + 0.5));

            /* show the picture! */
            video_display(is);

            /* update queue for next picture! */
            if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
                is->pictq_rindex = 0;
            }
            SDL_LockMutex(is->pictq_mutex);
            is->pictq_size--;
            SDL_CondSignal(is->pictq_cond);
            SDL_UnlockMutex(is->pictq_mutex);
        }
    } else {
        if(!is->quit){
            schedule_refresh(is, 100);
        }
    }
}

void alloc_picture(void *userdata) {
    VideoState *is = (VideoState *) userdata;
    VideoPicture *vp;

    slog(LVL_QUIET,ERROR,"alloc_picture called!");
    vp = &is->pictq[is->pictq_windex];

    if(vp->rawdata) {
        av_free(vp->rawdata);
    }

    if(is->scaled == false) { 
        is->scaledWidth = is->video_st->codec->width;
        is->scaledHeight = is->video_st->codec->height;
    }

    vp->width = is->scaledWidth;
    vp->height = is->scaledHeight;
    if(is->newTexture == true){
	is->newTexture = false;
    	if (is->TI->texture) {
                // Save the original texture for later
                is->originalTexture = is->TI->texture;
                //is->TI->texture=NULL;
        	//SDL_DestroyTexture(is->TI->texture);
    	}
    	slog(DEBUG,FULLDEBUG,"Creating video texture %s, size %dx%d / CT:%d",is->TI->name, is->TI->rect->w, is->TI->rect->h, is->newTexture);
    	is->TI->texture = SDL_CreateTexture(ph->renderer, SDL_PIXELFORMAT_YV12,
        	SDL_TEXTUREACCESS_STATIC , is->TI->rect->w, is->TI->rect->h);
    	//is->TI->texture = SDL_CreateTexture(ph->renderer, SDL_PIXELFORMAT_YV12,
        //	SDL_TEXTUREACCESS_STREAMING , is->TI->rect->w, is->TI->rect->h);
        ph->textureCount++;
        is->TI->active=true;
    	//SDL_Rect *r = is->TI->rect;
    	//r->w = vp->width;
    	//r->h = vp->height;
    	//r->x = is->videoPosX;
    	//r->y = is->videoPosY;
    }

    AVFrame* pFrameYUV = avcodec_alloc_frame();
    if (pFrameYUV == NULL)
       return;

    int numBytes = avpicture_get_size(AV_PIX_FMT_YUV420P, vp->width, vp->height);

    uint8_t* buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    avpicture_fill((AVPicture *) pFrameYUV, buffer, AV_PIX_FMT_YUV420P, vp->width, vp->height);

    vp->rawdata = pFrameYUV;

    SDL_LockMutex(is->pictq_mutex);
    vp->allocated = 1;
    SDL_CondSignal(is->pictq_cond);
    SDL_UnlockMutex(is->pictq_mutex);
}

int queue_picture(VideoState *is, AVFrame *pFrame, double pts) {
    VideoPicture *vp;
    //int dst_pic_fmt
    AVPicture pict;
    int i = 0;

    /* wait unitl we have space for a new pic */
    SDL_LockMutex(is->pictq_mutex);
    while (is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !is->quit) {
        SDL_CondWait(is->pictq_cond, is->pictq_mutex);
    }
    SDL_UnlockMutex(is->pictq_mutex);

    if (is->quit)
        return -1;

    // windex is set to 0 initially
    vp = &is->pictq[is->pictq_windex];

    /* allocate or resize the buffer ! */
    if (!is->TI->texture || is->newTexture == true){
// || vp->width != is->video_st->codec->width
//            || vp->height != is->video_st->codec->height) {

        vp->allocated = 0;
        /* we have to do this in the Main thread which owns the SDL window */
      	callWithData("video::alloc_picture", &i, is);

        /* wait until we have a picture allocated */
        SDL_LockMutex(is->pictq_mutex);
        while (!vp->allocated && !is->quit) {
            SDL_CondWait(is->pictq_cond, is->pictq_mutex);
        }
    }
    SDL_UnlockMutex(is->pictq_mutex);
    if (is->quit) {
        return -1;
    }

    /* We have a place to put our picture on the queue */
    if (vp->rawdata) {
        // Convert the image into YUV format that SDL uses
        sws_scale(is->sws_ctx, (uint8_t const * const *) pFrame->data,
                pFrame->linesize, 0, is->video_st->codec->height,
                vp->rawdata->data, vp->rawdata->linesize);

        vp->pts = pts;

        /* now we inform our display thread that we have a pic ready */
        if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) {
            is->pictq_windex = 0;
        }
        SDL_LockMutex(is->pictq_mutex);
        is->pictq_size++;
        SDL_UnlockMutex(is->pictq_mutex);
    }
    return 0;
}

double synchronize_video(VideoState *is, AVFrame *src_frame, double pts) {

    double frame_delay;

    if (pts != 0) {
        /* if we have pts, set video clock to it */
        is->video_clock = pts;
    } else {
        /* if we aren't given a pts, set it to the clock */
        pts = is->video_clock;
    }
    /* update the video clock */
    frame_delay = av_q2d(is->video_st->codec->time_base);
    /* if we are repeating a frame, adjust clock accordingly */
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    is->video_clock += frame_delay;
    return pts;
}
uint64_t global_video_pkt_pts = AV_NOPTS_VALUE;

/* These are called whenever we allocate a frame
 * buffer. We use this to store the global_pts in
 * a frame at the time it is allocated.
 */
int our_get_buffer(struct AVCodecContext *c, AVFrame *pic) {
    int ret = avcodec_default_get_buffer(c, pic);
    uint64_t *pts = av_malloc(sizeof(uint64_t));
    *pts = global_video_pkt_pts;
    pic->opaque = pts;
    return ret;
}

void our_release_buffer(struct AVCodecContext *c, AVFrame *pic) {
    if (pic)
        av_freep(&pic->opaque);
    avcodec_default_release_buffer(c, pic);
}

int video_thread(void *arg) {
    VideoState *is = (VideoState *) arg;
    AVPacket pkt1, *packet = &pkt1;
    int frameFinished;
    AVFrame *pFrame;

    double pts;

    pFrame = av_frame_alloc();

    slog(DEBUG,DEBUG,"Video thread started.");

    for (;;) {
        slog(TRACE,DEBUG,"Video thread loop : %d",packet->pts);
        if (packet_queue_get(&is->videoq, packet, 1) < 0) {
            // means we quit getting packets
            break;
        }

        pts = 0;

        // Save global pts to be stored in pFrame in first call
        global_video_pkt_pts = packet->pts;


        // Decode video frame
        avcodec_decode_video2(is->video_st->codec, pFrame, &frameFinished,
                packet);

        if (packet->dts == AV_NOPTS_VALUE && pFrame->opaque
                && *(uint64_t*) pFrame->opaque != AV_NOPTS_VALUE) {
            pts = *(uint64_t *) pFrame->opaque;
        } else if (packet->dts != AV_NOPTS_VALUE) {
            pts = packet->dts;
        } else {
            pts = 0;
        }
        pts *= av_q2d(is->video_st->time_base);

        // Did we get a video frame?
        if (frameFinished) {
            pts = synchronize_video(is, pFrame, pts);
            if (queue_picture(is, pFrame, pts) < 0) {
                break;
            }
        }
        av_free_packet(packet);
    }

    av_free(pFrame);
    return 0;
}

int audio_stream_component_open(VideoState *is, int stream_index) {
    AVFormatContext *ic = is->ic;
    AVCodecContext *codecCtx;
    AVCodec *codec;
    SDL_AudioSpec wanted_spec, spec;
    int64_t wanted_channel_layout = 0;
    int wanted_nb_channels;
    const int next_nb_channels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };

    if (stream_index < 0 || stream_index >= ic->nb_streams) {
        return -1;
    }

    codecCtx = ic->streams[stream_index]->codec;
    wanted_nb_channels = codecCtx->channels;
    if (!wanted_channel_layout
            || wanted_nb_channels
                    != av_get_channel_layout_nb_channels(
                            wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(
                wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }

    wanted_spec.channels = av_get_channel_layout_nb_channels(
            wanted_channel_layout);
    wanted_spec.freq = codecCtx->sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        slog(LVL_QUIET,ERROR, "Invalid sample rate or channel count!");
        return -1;
    }
    wanted_spec.format = AUDIO_S16SYS; 
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = is;

    while (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
        slog(LVL_QUIET,ERROR, "SDL_OpenAudio (%d channels): %s",
                wanted_spec.channels, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            fprintf(stderr,
                    "No more channel combinations to tyu, audio open failed\n");
            return -1;
        }
        wanted_channel_layout = av_get_default_channel_layout(
                wanted_spec.channels);
    }

    if (spec.format != AUDIO_S16SYS) {
        fprintf(stderr, "SDL advised audio format %d is not supported!\n",
                spec.format);
        return -1;
    }

    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            fprintf(stderr, "SDL advised channel count %d is not supported!\n",
                    spec.channels);
            return -1;
        }
    }

    is->audio_hw_buf_size = spec.size;

    is->audio_src_fmt = is->audio_tgt_fmt = AV_SAMPLE_FMT_S16;
    is->audio_src_freq = is->audio_tgt_freq = spec.freq;
    is->audio_src_channel_layout = is->audio_tgt_channel_layout =
            wanted_channel_layout;
    is->audio_src_channels = is->audio_tgt_channels = spec.channels;

    codec = avcodec_find_decoder(codecCtx->codec_id);
    if (!codec || (avcodec_open2(codecCtx, codec, NULL) < 0)) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
    }
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (codecCtx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audioStream = stream_index;
        is->audio_st = ic->streams[stream_index];
        is->audio_buf_size = 0;
        is->audio_buf_index = 0;
        memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
        packet_queue_init(&is->audioq);
        SDL_PauseAudio(0); 
        break;
    default:
        break;
    }

    return 0;
}

int video_stream_component_open(VideoState *is, int stream_index) {
    AVFormatContext *pFormatCtx = is->ic;
    AVCodecContext *codecCtx;
    AVCodec *codec;

    if (stream_index < 0 || stream_index >= pFormatCtx->nb_streams) {
        return -1;
    }

    // Get a pointer to the codec context for the video stream
    codecCtx = pFormatCtx->streams[stream_index]->codec;

    codec = avcodec_find_decoder(codecCtx->codec_id);
    if (!codec || (avcodec_open2(codecCtx, codec, NULL) < 0)) {
        slog(LVL_QUIET,ERROR, "Unsupported codec!");
        return -1;
    }

    switch (codecCtx->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        is->videoStream = stream_index;
        is->video_st = pFormatCtx->streams[stream_index];
        // Rescale here
        if(is->scaled == true) {
            is->sws_ctx = sws_getContext(is->video_st->codec->width,
                is->video_st->codec->height, is->video_st->codec->pix_fmt,
                is->scaledWidth, is->scaledHeight,
                AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
        } else {
            is->sws_ctx = sws_getContext(is->video_st->codec->width,
                is->video_st->codec->height, is->video_st->codec->pix_fmt,
                is->video_st->codec->width, is->video_st->codec->height,
                AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);
        }

        is->frame_timer = (double) av_gettime() / 1000000.0;
        is->frame_last_delay = 40e-3;

        packet_queue_init(&is->videoq);
        is->video_tid = SDL_CreateThread(video_thread, "video_thread", is);

        codecCtx->get_buffer2 = avcodec_default_get_buffer2;
            //our_get_buffer;
        //codecCtx->release_buffer = our_release_buffer;
        break;
    default:
        break;
    }
    return 0;
}

int decode_thread(void *arg) {
    VideoState *is = (VideoState *) arg;
    AVFormatContext *pFormatCtx = NULL;
    AVPacket pkt1, *packet = &pkt1;

    int video_index = -1;
    int audio_index = -1;
    int i;
    is->quit = false;

    is->videoStream = -1;
    is->audioStream = -1;

    AVIOInterruptCB interupt_cb;

    // will interrupt blocking functions if we quit!
    interupt_cb.callback = decode_interrupt_cb;
    interupt_cb.opaque = is;

    if (avio_open2(&is->io_ctx, is->filename, 0, &interupt_cb, NULL)) {
        slog(LVL_QUIET,ERROR, "Cannot open I/O for %s\n", is->filename);
	is->quit = true;
        return -1;
    }

    //Open video file
    if (avformat_open_input(&pFormatCtx, is->filename, NULL, NULL) != 0) {
	is->quit = true;
        return -1; //Couldn't open file
    }

    is->ic = pFormatCtx;

    //Retrieve stream infomation
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
	is->quit = true;
        return -1; // Couldn't find stream information
    }

    //Dump information about file onto standard error
    if(ph->loglevel >= DEBUG){
        av_dump_format(pFormatCtx, 0, is->filename, 0);
    }

    //Find the first video stream
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->coder_type == AVMEDIA_TYPE_VIDEO
                && video_index < 0) {
            video_index = i;
        }

        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
                && audio_index < 0) {
            audio_index = i;
        }
    }

    if (audio_index >= 0) {
	slog(DEBUG,DEBUG,"Opening audio stream");
        audio_stream_component_open(is, audio_index);
    }

    if (video_index >= 0) {
	slog(DEBUG,DEBUG,"Opening video stream");
        video_stream_component_open(is, video_index);
    }

    if (is->videoStream >= 0 || is->audioStream > 0) {
        if(is->audioStream < 0){
            slog(LVL_INFO,INFO,"Could not open audio stream / no audio stream found.");
            is->hasAudio = false;
        } else {
            is->hasAudio = true;
        }
        
        if(is->videoStream < 0){
            is->hasVideo = false;
            slog(LVL_INFO,WARN,"Cloud not open video stream");
        }

        //main decode loop
        for (;;) {
            if (is->quit || ph->playVideo == false) {
                break;
            }
            if (is->pause) {
                SDL_Delay(100); // sleep then start the loop over
                continue;
            }
            //seek stuff goes here
            if (is->audioq.size > MAX_AUDIO_SIZE || is->videoq.size > MAX_VIDEO_SIZE) {
                SDL_Delay(10);
                continue;
            }
            if (av_read_frame(is->ic, packet) < 0) {
                if (is->ic->pb->error == 0) {
                    SDL_Delay(100); /* no error; wait for user input */
                    slog(DEBUG,DEBUG,"av_read_frame failed with no error. : %d",is->read_frame_fail);
                    is->read_frame_fail++;
                    // Find a proper solution for this / or test if video breaks on bad network?
                    if(is->read_frame_fail > VIDEO_FAIL_PACKETS)
                        is->quit = true;
                    continue;
                } else {
                    // is->quit = true;
                    break;
                }
            }
            // Is this a packet from the video stream?
            if (packet->stream_index == is->videoStream) {
                packet_queue_put(&is->videoq, packet);
            } else if (packet->stream_index == is->audioStream) {
                packet_queue_put(&is->audioq, packet);
            } else {
                av_free_packet(packet);
            }
        }
        SDL_CloseAudio();

    } else {
        slog(LVL_QUIET,ERROR, "%s: could not open audio and video (unsupported codec?)", is->filename);
    }
    slog(DEBUG,DEBUG,"Video decode thread finished.");
        is->quit = true;
        SDL_CondSignal(is->videoq.cond);
        SDL_CondSignal(is->audioq.cond);
    //    call("video::reset",&i,NULL);

    return 0;
}
