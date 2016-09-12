/* -*- c++ -*- */
/* 
 * Copyright 2016 <+YOU OR YOUR COMPANY+>.
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
//#include <volk/volk.h>
#include <gnuradio/block.h>

#define DEMOD_HISTORY_DEPTH      3
#define REQUIRED_PREAMBLE_DEPTH  4
#define REQUIRED_SFD_CHIRPS      2
#define LORA_SFD_TOLERANCE       2

namespace gr {
  namespace lora {

    enum demod_state_t {
      S_RESET,
      S_PREFILL,
      S_DETECT_PREAMBLE,
      S_DETECT_SYNC,
      S_TUNE_SYNC,
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
      static sptr make( int bandwidth,
                        unsigned short spreading_factor,
                        unsigned short code_rate);
    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_DEMOD_H */

