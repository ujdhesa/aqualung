/*                                                     -*- linux-c -*-
    Copyright (C) 2005 Tom Szilagyi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#ifndef AQUALUNG_DEC_MAC_H
#define AQUALUNG_DEC_MAC_H

#include "../rb.h"
#include "file_decoder.h"


/* Decoding buffer size for Monkey's Audio */
#define MAC_BUFSIZE 9216
/* size of ringbuffer for decoded Monkey's Audio data (in frames) */
#define RB_MAC_SIZE 262144


typedef struct _mac_pdata_t {
        rb_t * rb;
	void * decompress; /* (IAPEDecompress *) */
	unsigned int sample_rate;
	unsigned int bits_per_sample;
	unsigned int bitrate;
	unsigned int channels;
	unsigned int length_in_ms;
	unsigned int block_align;
	unsigned int compression_level;
	int swap_bytes;
        int is_eos;
} mac_pdata_t;


#ifdef __cplusplus
extern "C" {
#endif

decoder_t * mac_decoder_init(file_decoder_t * fdec);
void mac_decoder_destroy(decoder_t * dec);
int mac_decoder_open(decoder_t * dec, char * filename);
void mac_decoder_send_metadata(decoder_t * dec );
void mac_decoder_close(decoder_t * dec);
unsigned int mac_decoder_read(decoder_t * dec, float * dest, int num);
void mac_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* AQUALUNG_DEC_MAC_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

