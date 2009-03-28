/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the TRUEHD output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_TRUEHD_H
#define __P_TRUEHD_H

#include "os.h"

#include <vector>

#include "common.h"
#include "pr_generic.h"
#include "truehd_common.h"

class truehd_packetizer_c: public generic_packetizer_c {
protected:
  bool m_first_frame;
  truehd_frame_t m_first_truehd_header;

  int64_t m_samples_output;
  truehd_parser_c m_parser;
  std::vector<truehd_frame_cptr> m_frames;

public:
  truehd_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, int sampling_rate, int channels) throw (error_c);
  virtual ~truehd_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();
  virtual void flush();

  virtual const char *get_format_name() {
    return "TrueHD";
  }

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, string &error_message);

protected:
  virtual void adjust_header_values(truehd_frame_cptr &frame);
};

#endif // __P_TRUEHD_H