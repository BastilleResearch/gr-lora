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


#ifndef INCLUDED_LORA_DECODE_H
#define INCLUDED_LORA_DECODE_H

#include <lora/api.h>
#include <gnuradio/block.h>
#include <lora/lora.h>

#define SYMBOL_TIMEOUT_COUNT   256

namespace gr {
  namespace lora {

    /*!
     * \brief <+description of block+>
     * \ingroup lora
     *
     */
    class LORA_API decode : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<decode> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of lora::decode.
       *
       * To avoid accidental use of raw pointers, lora::decode's
       * constructor is in a private implementation
       * class. lora::decode::make is the public interface for
       * creating new instances.
       */
      static sptr make( short spreading_factor,
                        short code_rate,
                        bool  low_data_rate,
                        bool  header);
    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_DECODE_H */

