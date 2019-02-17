//
//  TranscodePipeline.cpp
//  live_transcoder
//
//  Created by Guy.Jacubovski on 31/12/2018.
//  Copyright © 2018 Kaltura. All rights reserved.
//

#include "TranscodePipeline.h"
#include "logger.h"


int init_transcoding_context(struct TranscodeContext *pContext,struct AVStream* pStream)
{
    pContext->inputs=0;
    pContext->outputs=0;
    pContext->filters=0;
    pContext->inputStream=pStream;
    
    struct TranscoderCodecContext *pDecoderContext=&pContext->decoder[0];
    init_decoder(pDecoderContext,pStream);
    
    return 0;
}



int encodeFrame(struct TranscodeContext *pContext,int encoderId,int outputId,AVFrame *pFrame) {
 

    int ret=0;
    
    struct TranscoderCodecContext* pEncoder=&pContext->encoder[encoderId];
    ret=send_encode_frame(pEncoder,pFrame);
    
    while (ret >= 0) {
        AVPacket *pOutPacket = av_packet_alloc();
        
        ret = receive_encoder_packet(pEncoder,pOutPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pOutPacket);
            return 0;
        }
        else if (ret < 0)
        {
            logger(CATEGORY_DEFAULT, AV_LOG_ERROR,"Error during decoding");
            return -1;
        }
        
        logger(CATEGORY_DEFAULT,AV_LOG_ERROR,"[%d] encoded frame for output %d: pts=%s (%s) size=%d",
               encoderId,
               outputId,
               av_ts2str(pOutPacket->pts), av_ts2timestr(pOutPacket->pts, &pEncoder->ctx->time_base),
               pOutPacket->size);
        
        
        send_output_packet(pContext->output[outputId],pOutPacket);
        
        av_packet_free(&pOutPacket);
    }
    return 0;
}

int OnDecodedFrame(struct TranscodeContext *pContext,AVCodecContext* pDecoderContext,const AVFrame *pFrame)
{
    
    if (pDecoderContext->codec_type==AVMEDIA_TYPE_VIDEO) {
        
        logger(CATEGORY_DEFAULT,AV_LOG_ERROR,"decoded video: pts=%s (%s), frame type=%s;width=%d;height=%d",
               av_ts2str(pFrame->pts), av_ts2timestr(pFrame->pts, &pDecoderContext->time_base),
               pict_type_to_string(pFrame->pict_type),pFrame->width,pFrame->height);
        
        //return 0;
        //  printf("saving frame %3d\n", pDecoderContext->frame_number);
    } else {
        logger(CATEGORY_DEFAULT,AV_LOG_ERROR,"decoded audio: pts=%s (%s);channels=%d;sample rate=%d; length=%d; format=%d ",
               av_ts2str(pFrame->pts), av_ts2timestr(pFrame->pts, &pDecoderContext->time_base),
               pFrame->channels,pFrame->sample_rate,pFrame->nb_samples,pFrame->format);
        
        return 0;
        
    }
    
    for (int filterId=0;filterId<pContext->filters;filterId++) {
        struct TranscodeFilter *pFilter=&pContext->filter[filterId];
        send_filter_frame(pFilter,pFrame);
        
        int ret=0;
        while (ret >= 0) {
            AVFrame *pOutFrame = av_frame_alloc();
            
            ret = receive_filter_frame(pFilter,pOutFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&pOutFrame);
                return 0;
            }
            else if (ret < 0)
            {
                logger(CATEGORY_DEFAULT,AV_LOG_ERROR,"Error during decoding");
                return -1;
            }
            logger(CATEGORY_DEFAULT,AV_LOG_ERROR,"filtered video: pts=%s (%s), frame type=%s;width=%d;height=%d",
                   av_ts2str(pOutFrame->pts), av_ts2timestr(pOutFrame->pts, &pDecoderContext->time_base),
                   pict_type_to_string(pOutFrame->pict_type),pOutFrame->width,pOutFrame->height);
            
            for (int outputId=0;outputId<pContext->outputs;outputId++) {
                struct TranscodeOutput *pOutput=pContext->output[outputId];
                if (pOutput->filter==filterId){
                    logger(CATEGORY_DEFAULT,AV_LOG_ERROR,"sending video from filter %d to encoder %d for output %s",filterId,pOutput->encoder,pOutput->name);
                    encodeFrame(pContext,pOutput->encoder,outputId,pFrame);
                }
            }
            av_frame_free(&pOutFrame);
        }
    }
    return 0;
}

int decodePacket(struct TranscodeContext *transcodingContext,const AVPacket* pkt) {
    
    int ret;
    
    int stream_index = pkt->stream_index;
    
    logger(CATEGORY_DEFAULT,AV_LOG_DEBUG, "Send packet from stream_index %u to decoder",stream_index);
    
    struct TranscoderCodecContext* pDecoder=&transcodingContext->decoder[stream_index];
    

    ret = send_decoder_packet(pDecoder, pkt);
    if (ret < 0) {
        logger(CATEGORY_DEFAULT,AV_LOG_ERROR, "Error sending a packet for decoding");
        return ret;
    }
    
    while (ret >= 0) {
        AVFrame *pFrame = av_frame_alloc();
        
        ret = receive_decoder_frame(pDecoder, pFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&pFrame);
            return 0;
        }
        else if (ret < 0)
        {
            logger(CATEGORY_DEFAULT,AV_LOG_ERROR,"Error during decoding");
            return ret;
        }
        OnDecodedFrame(transcodingContext,pDecoder->ctx,pFrame);
        
        av_frame_free(&pFrame);
    }
    return 0;
}

int convert_packet(struct TranscodeContext *pContext ,struct AVPacket* packet)
{
    bool shouldDecode=false;
    for (int i=0;i<pContext->outputs;i++) {
        struct TranscodeOutput *pOutput=pContext->output[i];
        if (pOutput->codec_type==pContext->inputStream->codecpar->codec_type)
        {
            if (pOutput->passthrough)
            {
                send_output_packet(pOutput,packet);
            }
            else
            {
                shouldDecode=true;
            }
        }
    }
    if (shouldDecode) {
       return decodePacket(pContext,packet);
    }
    return 0;
}



int add_output(struct TranscodeContext* pContext, struct TranscodeOutput * pOutput)
{
    struct TranscoderCodecContext *pDecoderContext=&pContext->decoder[0];
    struct AVStream*  pStream= pContext->inputStream;
    
    if (!pOutput->passthrough && pOutput->codec_type==AVMEDIA_TYPE_VIDEO) {
        char config[2048];
        sprintf(config,"scale=%dx%d:force_original_aspect_ratio=decrease",pOutput->width,pOutput->height);
        
        struct TranscoderFilter* pFilter=NULL;
        pOutput->filter=-1;
        for (int selectedFilter=0; selectedFilter<pContext->filters;selectedFilter++) {
            pFilter=&pContext->filter[selectedFilter];
            if (strcmp(pFilter->config,config)==0) {
                pOutput->filter=selectedFilter;
                logger(CATEGORY_DEFAULT,AV_LOG_ERROR,"Output %s - Resuing existing filter %s",pOutput->name,config);
            }
        }
        if ( pOutput->filter==-1) {
            pOutput->filter=pContext->filters++;
            pFilter=&pContext->filter[pOutput->filter];
            init_filter(pFilter,pStream,pDecoderContext->ctx,config);
            logger(CATEGORY_DEFAULT,AV_LOG_ERROR,"Output %s - Created new  filter %s",pOutput->name,config);
        }
        
        pOutput->encoder=pContext->encoders++;
        struct TranscoderCodecContext* pCodec=&pContext->encoder[pOutput->encoder];
        
        int width=av_buffersink_get_w(pFilter->sink_ctx);
        int height=av_buffersink_get_h(pFilter->sink_ctx);
        AVRational frameRate=pStream->avg_frame_rate;
        
        enum AVPixelFormat format= av_buffersink_get_format(pFilter->sink_ctx);
        init_video_encoder(pCodec, pDecoderContext->ctx->sample_aspect_ratio,format,frameRate,width,height,pOutput->vid_bitrate*1000);
        logger(CATEGORY_DEFAULT,AV_LOG_ERROR,"Output %s - Added encoder %d bitrate=%d",pOutput->name,pOutput->encoder,pOutput->vid_bitrate*1000);

        
    }
    
    
    /*
    if (pStream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO) {
        continue;
        
        struct TranscoderFilter* pFilter=&pContext->filter[pContext->filters++];
        
        init_filter(pFilter,pStream,pDecoderContext->ctx,"aformat=sample_fmts=fltp:channel_layouts=stereo:sample_rates=44100");
        struct TranscoderCodecContext* pCodec=&pContext->encoder[pContext->encoders++];
        
        //init_audio_encoder(pCodec, pDecoderContext->ctx->sample_aspect_ratio,format,frameRate,width,height,1000*1000);
    }
    
        
        
    }
    */
    
    pContext->output[pContext->outputs++]=pOutput;
    return 0;
}

