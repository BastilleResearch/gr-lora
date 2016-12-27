/* -*- c++ -*- */
/* 
 * Copyright 2016 Bastille Networks.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */


#ifndef INCLUDED_LORA_DEMOD_H
#define INCLUDED_LORA_DEMOD_H

#include <lora/api.h>
#include <gnuradio/block.h>

#define DEMOD_HISTORY_DEPTH        3
#define REQUIRED_PREAMBLE_CHIRPS   4
#define REQUIRED_SFD_CHIRPS        2
#define LORA_SFD_TOLERANCE         1
#define LORA_PREAMBLE_TOLERANCE    1
#define DEMOD_SYNC_RECOVERY_COUNT  (8-REQUIRED_PREAMBLE_CHIRPS)+(2-REQUIRED_SFD_CHIRPS)+4

namespace gr {
  namespace lora {

    enum demod_state_t {
      S_RESET,
      S_PREFILL,
      S_DETECT_PREAMBLE,
      S_SFD_SYNC,
      S_READ_HEADER,
      S_READ_PAYLOAD,
      S_OUT
    };

    /*!
     * \brief <+description of block+>
     * \ingroup lora
     *
     */
    class LORA_API demod : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<demod> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of lora::demod.
       *
       * To avoid accidental use of raw pointers, lora::demod's
       * constructor is in a private implementation
       * class. lora::demod::make is the public interface for
       * creating new instances.
       */
      static sptr make( unsigned short spreading_factor,
                        bool  low_data_rate,
                        float beta,
                        unsigned short fft_factor);
    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_DEMOD_H */

