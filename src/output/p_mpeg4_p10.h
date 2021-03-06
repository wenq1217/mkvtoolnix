/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the MPEG4 part 10 video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_MPEG4_P10_H
#define __P_MPEG4_P10_H

#include "common/common_pch.h"

#include "output/p_video.h"

class mpeg4_p10_video_packetizer_c: public video_packetizer_c {
protected:
  int m_nalu_size_len_src, m_nalu_size_len_dst;
  int64_t m_max_nalu_size;

public:
  mpeg4_p10_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, double fps, int width, int height);
  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("AVC/h.264") : "AVC/h.264";
  }

protected:
  virtual void extract_aspect_ratio();
  virtual void setup_nalu_size_len_change();
  virtual void change_nalu_size_len(packet_cptr packet);
};

#endif  // __P_MPEG4_P10_H
