//
//  LOGGER.c
//  live_transcoder
//
//  Created by Guy.Jacubovski on 31/12/2018.
//  Copyright © 2018 Kaltura. All rights reserved.
//

#include <stdio.h>

#include <stdbool.h>
#include <sys/time.h>
#include "logger.h"
#include "utils.h"
#include <pthread.h>


static int logLevel =AV_LOG_VERBOSE;

const   char* getLevel(int level) {
    switch(level){
        case AV_LOG_PANIC: return "PANIC";
        case AV_LOG_FATAL: return "FATAL";
        case AV_LOG_ERROR: return "ERROR";
        case AV_LOG_WARNING: return "WARN";
        case AV_LOG_INFO: return "INFO";
        case AV_LOG_VERBOSE: return "VERBOSE";
        case AV_LOG_DEBUG: return "DEBUG";
    }
    return "";
}
void logger2(const char* category,const char* subcategory,int level,const char *fmt, bool newLine, va_list args)
{    
    const char* levelStr=getLevel(level);
    
    int64_t now=getClock64();
    time_t epoch=now/1000000;
    struct tm *gm = localtime(&epoch);
    
    
    char buf[25];
    strftime(buf, 25, "%Y-%m-%dT%H:%M:%S",gm);
    
    
    fprintf( stderr, "%s.%03d %s:%s %s [%p] ",buf,(int)( (now % 1000000)/1000 ),category,subcategory!=NULL ? subcategory : "", levelStr,pthread_self());
    if (args!=NULL) {
        vfprintf( stderr, fmt, args );
    } else {
        fprintf(stderr,"%s",fmt);
    }
    if (newLine) {
        fprintf( stderr, "\n" );
    }
}



void logger1(char* category,int level,const char *fmt, ...)
{
    va_list args;
    va_start( args, fmt );
    logger2("TRANSCODER",category,level,fmt,true,args);
    va_end( args );
}


/*
static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    
    LOGGER(AV_LOG_DEBUG,"%s:  stream_index:%d  pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s flags:%d\n",
           tag,
           pkt->stream_index,
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->flags);
}*/



/*
const char *av_default_item_name(void *ptr)
{
    return (*(AVClass **) ptr)->class_name;
}

AVClassCategory av_default_get_category(void *ptr)
{
    return (*(AVClass **) ptr)->category;
}*/

void ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vargs)
{
    if (level>logLevel)
        return;
    
    char tmp[1024];
    int prefix=1;
    av_log_format_line(ptr,level,fmt,vargs,tmp,sizeof(tmp), &prefix);
    logger2 (CATEGORY_FFMPEG, ptr!=NULL ? av_default_item_name(ptr) : "",level,tmp,false,NULL);
}


void log_init(int level)
{
    logLevel=level;
}

void init_ffmpeg_log_level(int logLevel)
{
    av_log_set_level(logLevel);
    av_log_set_callback(ffmpeg_log_callback);

}
int get_log_level(char* category,int level)
{
    return logLevel;
}
void loggerFlush() 
{
    fflush(stderr);
}
