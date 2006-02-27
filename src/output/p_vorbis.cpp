/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   Vorbis packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "common.h"
#include "matroska.h"
#include "output_control.h"
#include "p_vorbis.h"

using namespace libmatroska;

vorbis_packetizer_c::vorbis_packetizer_c(generic_reader_c *_reader,
                                         unsigned char *d_header,
                                         int l_header,
                                         unsigned char *d_comments,
                                         int l_comments,
                                         unsigned char *d_codecsetup,
                                         int l_codecsetup,
                                         track_info_c &_ti)
  throw (error_c):
  generic_packetizer_c(_reader, _ti),
  last_bs(0), samples(0), last_samples_sum(0), last_timecode(0),
  timecode_offset(0) {

  int i;
  ogg_packet ogg_headers[3];

  headers.push_back(memory_cptr(new memory_c((unsigned char *)
                                             safememdup(d_header, l_header),
                                             l_header)));
  headers.push_back(memory_cptr(new memory_c((unsigned char *)
                                             safememdup(d_comments,
                                                        l_comments),
                                             l_comments)));
  headers.push_back(memory_cptr(new memory_c((unsigned char *)
                                             safememdup(d_codecsetup,
                                                        l_codecsetup),
                                             l_codecsetup)));

  memset(ogg_headers, 0, 3 * sizeof(ogg_packet));
  ogg_headers[0].packet = headers[0]->get();
  ogg_headers[1].packet = headers[1]->get();
  ogg_headers[2].packet = headers[2]->get();
  ogg_headers[0].bytes = l_header;
  ogg_headers[1].bytes = l_comments;
  ogg_headers[2].bytes = l_codecsetup;
  ogg_headers[0].b_o_s = 1;
  ogg_headers[1].packetno = 1;
  ogg_headers[2].packetno = 2;
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);
  for (i = 0; i < 3; i++)
    if (vorbis_synthesis_headerin(&vi, &vc, &ogg_headers[i]) < 0)
      throw error_c("Error: vorbis_packetizer: Could not extract the "
                    "stream's parameters from the first packets.\n");

  set_track_type(track_audio);
  if (use_durations)
    set_track_default_duration((int64_t)(1024000000000.0 *
                                            ti.async.linear / vi.rate));
  ti.async.displacement = initial_displacement;
}

vorbis_packetizer_c::~vorbis_packetizer_c() {
  vorbis_info_clear(&vi);
  vorbis_comment_clear(&vc);
}

void
vorbis_packetizer_c::set_headers() {
  memory_cptr codecprivate;

  set_codec_id(MKV_A_VORBIS);

  codecprivate = lace_memory_xiph(headers);
  set_codec_private(codecprivate->get(), codecprivate->get_size());

  set_audio_sampling_freq((float)vi.rate);
  set_audio_channels(vi.channels);

  generic_packetizer_c::set_headers();
}

/*
   Some notes - processing is straight-forward if no AV synchronization
   is needed - the packet is simply stored in the Matroska file.
   Unfortunately things are not that easy if AV sync is done. For a
   negative displacement packets are simply discarded if their timecode
   is set before the displacement. For positive displacements the packetizer
   has to generate silence packets and put them into the Matroska file first.
*/
int
vorbis_packetizer_c::process(packet_cptr packet) {
  unsigned char zero[2];
  ogg_packet op;
  int64_t this_bs, samples_here, samples_needed, expected_timecode;
  int64_t chosen_timecode;

  debug_enter("vorbis_packetizer_c::process");

  // Remember the very first timecode we received.
  if ((samples == 0) && (packet->timecode > 0))
    timecode_offset = packet->timecode;

  // Positive displacement, first packet? Well then lets create silence.
  if ((samples == 0) && (initial_displacement > 0)) {
    // Create a fake packet so we can use vorbis_packet_blocksize().
    zero[0] = 0;
    zero[1] = 0;
    memset(&op, 0, sizeof(ogg_packet));
    op.packet = zero;
    op.bytes = 2;

    // Calculate how many samples we have to create.
    samples_needed = vi.rate * initial_displacement / 1000000000;

    this_bs = vorbis_packet_blocksize(&vi, &op);
    samples_here = (this_bs + last_bs) / 4;
    while ((samples + samples_here) < samples_needed) {
      samples += samples_here;
      last_bs = this_bs;
      samples_here = (this_bs + last_bs) / 4;
      add_packet(new packet_t(new memory_c(zero, 2, false),
                              samples * 1000000000 / vi.rate,
                              samples_here * 1000000000 / vi.rate));
    }
    last_samples_sum = samples;
  }

  // Update the number of samples we have processed so that we can
  // calculate the timecode on the next call.
  op.packet = packet->data->get();
  op.bytes = packet->data->get_size();
  this_bs = vorbis_packet_blocksize(&vi, &op);
  samples_here = (this_bs + last_bs) / 4;
  last_bs = this_bs;
  samples += samples_here;

  expected_timecode = last_timecode + last_samples_sum * 1000000000 / vi.rate +
    timecode_offset;
  packet->timecode += initial_displacement;
  if (initial_displacement < 0)
    expected_timecode += initial_displacement;

  if (packet->timecode > (expected_timecode + 100000000)) {
    chosen_timecode = packet->timecode;
    packet->duration = packet->timecode -
      (last_timecode + last_samples_sum * 1000000000 / vi.rate +
       timecode_offset);
    last_timecode = packet->timecode;
    last_samples_sum = 0;
  } else {
    chosen_timecode = expected_timecode;
    packet->duration = (int64_t)(samples_here * 1000000000 * ti.async.linear /
                                 vi.rate);
  }

  last_samples_sum += samples_here;

  // Handle the linear sync - simply multiply with the given factor.
  chosen_timecode = (int64_t)((double)chosen_timecode * ti.async.linear);

  // If a negative sync value was used we may have to skip this packet.
  if (chosen_timecode < 0) {
    debug_leave("vorbis_packetizer_c::process");
    return FILE_STATUS_MOREDATA;
  }

  mxverb(2, "Vorbis: samples_here at " LLD " (orig " LLD " expected " LLD "): "
         LLD " (last_samples_sum: " LLD ")\n",
         chosen_timecode, packet->timecode, expected_timecode,
         samples_here, last_samples_sum);
  packet->timecode = chosen_timecode;
  add_packet(packet);

  debug_leave("vorbis_packetizer_c::process");

  return FILE_STATUS_MOREDATA;
}

void
vorbis_packetizer_c::dump_debug_info() {
  mxdebug("vorbis_packetizer_c: queue: %u\n",
          (unsigned int)packet_queue.size());
}

connection_result_e
vorbis_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                    string &error_message) {
  vorbis_packetizer_c *vsrc;

  vsrc = dynamic_cast<vorbis_packetizer_c *>(src);
  if (vsrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  connect_check_a_samplerate(vi.rate, vsrc->vi.rate);
  connect_check_a_channels(vi.channels, vsrc->vi.channels);
  if ((headers[2]->get_size() != vsrc->headers[2]->get_size()) ||
      memcmp(headers[2]->get(), vsrc->headers[2]->get(),
             headers[2]->get_size())) {
    error_message = "The Vorbis codebooks are different; such tracks cannot "
      "be concatenated without reencoding";
    return CAN_CONNECT_NO_FORMAT;
  }
  return CAN_CONNECT_YES;
}
