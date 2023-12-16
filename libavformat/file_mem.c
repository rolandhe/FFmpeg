/*
 * memory stream
 * Copyright (c) 2023 Roland he
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/avstring.h"
#include "libavutil/internal.h"
#include "libavutil/opt.h"
#include "avformat.h"

#include <stdlib.h>
#include "url.h"

typedef struct DataBuffer {
    char * buffer;
    int size;
} DataBuffer;

typedef struct FileMemContext {
    const AVClass *class;
    DataBuffer * pBuffer;
    int ctrlPoint;
} FileMemContext;



static const AVOption file_mem_options[] = {
        { NULL }
};

static const AVClass file_mem_class = {
        .class_name = "filemem",
        .item_name  = av_default_item_name,
        .option     = file_mem_options,
        .version    = LIBAVUTIL_VERSION_INT,
};


static int file_mem_read(URLContext *h, unsigned char *buf, int size){
    FileMemContext *c = h->priv_data;

    int remaining_size = c->pBuffer->size  - c->ctrlPoint;

    if (remaining_size <= 0) {
        return AVERROR_EOF;
    }

    int copy_size = FFMIN(size, remaining_size);
    memcpy(buf, c->pBuffer->buffer + c->ctrlPoint, copy_size);
    c->ctrlPoint += copy_size;
    return copy_size;
}

static int file_mem_write(URLContext *h, const unsigned char *buf, int size){
    FileMemContext *c = h->priv_data;

    if(!c->pBuffer->buffer){
        int buffer_size = 32768*2;
        if(buffer_size < size){
            buffer_size += size;
        }
        c->pBuffer->buffer = malloc(buffer_size);
        c->ctrlPoint = buffer_size;
    }

    int remaining = c->ctrlPoint - c->pBuffer->size;

    if (remaining < size){
        int new_buff_size = c->ctrlPoint * 2;
        if (new_buff_size < size) {
            new_buff_size += size;
        }
        char * new_buff = malloc(new_buff_size  );
        memcpy(new_buff, c->pBuffer->buffer, c->pBuffer->size);

        free(c->pBuffer->buffer);
        c->pBuffer->buffer = new_buff;
        c->ctrlPoint = new_buff_size;
    }

    memcpy(c->pBuffer->buffer + c->pBuffer->size, buf, size);
    c->pBuffer->size += size;
    return size;
}

#if CONFIG_FILEMEM_PROTOCOL

static int file_mem_open(URLContext *h, const char *filename, int flags)
{
    FileMemContext *c = h->priv_data;
    int64_t result;
    char *final;
    av_strstart(filename, "filemem:", &filename);
    char * ext = av_stristr(filename,".");
    if(!ext){
        result = strtoll(filename, &final, 16);
    } else {
        char tmp[255] = {0};
        strncpy(tmp,filename,ext - filename);
        result = strtoll(tmp, &final, 16);
    }

    c->pBuffer = (DataBuffer *)result;
    if(flags & AVIO_FLAG_WRITE) {
        c->ctrlPoint = c->pBuffer->size;
        c->pBuffer->size = 0;
    } else {
        c->ctrlPoint = 0;
    }


    h->is_streamed = 1;
    return 0;
}

const URLProtocol ff_filemem_protocol = {
    .name                = "filemem",
    .url_open            = file_mem_open,
    .url_read            = file_mem_read,
    .url_write           = file_mem_write,
    .priv_data_size      = sizeof(FileMemContext),
    .priv_data_class     = &file_mem_class,
    .default_whitelist   = "crypto,data"
};

#endif /* CONFIG_FILEMEM_PROTOCOL */
