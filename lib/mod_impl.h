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

#ifndef INCLUDED_LORA_MOD_IMPL_H
#define INCLUDED_LORA_MOD_IMPL_H

#include <vector>
#include <complex>
#include <fstream>
#include <volk/volk.h>
#include <lora/mod.h>

#define NUM_PREAMBLE_CHIRPS   8
#define LORA_SYNCWORD0        3
#define LORA_SYNCWORD1        4

namespace gr {
  namespace lora {

    class mod_impl : public mod
    {
     private:
      pmt::pmt_t d_in_port;

      unsigned char d_sf;
      unsigned char d_sync_word;

      unsigned short d_fft_size;
      unsigned char  d_interleaver_size;

      std::vector<gr_complex> d_upchirp;
      std::vector<gr_complex> d_downchirp;

      std::vector<gr_complex> d_iq_out;

      std::ofstream f_mod;

     public:
      mod_impl( short spreading_factor, unsigned char d_sync_word);
      ~mod_impl();

      void modulate (pmt::pmt_t msg);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);
    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_MOD_IMPL_H */

