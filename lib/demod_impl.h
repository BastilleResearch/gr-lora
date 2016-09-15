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

#ifndef INCLUDED_LORA_DEMOD_IMPL_H
#define INCLUDED_LORA_DEMOD_IMPL_H

#include <cmath>
#include <cstdlib>
#include <vector>
#include <queue>
#include <complex>
#include <fstream>
#include <gnuradio/fft/fft.h>
#include <gnuradio/fft/window.h>
#include <volk/volk.h>
#include "lora/demod.h"

namespace gr {
  namespace lora {

    class demod_impl : public demod
    {
     private:
      pmt::pmt_t d_out_port;

      demod_state_t   d_state;
      float           d_bw;
      unsigned short  d_sf;
      unsigned short  d_cr;

      unsigned short  d_fft_size;
      unsigned short  d_overlaps;
      unsigned short  d_offset;

      float           d_power;
      float           d_threshold;
      bool            d_squelched;

      unsigned short  d_preamble_idx;
      unsigned short  d_sfd_idx;
      std::vector<unsigned short>  d_argmax_history;
      std::vector<unsigned short>  d_sfd_history;

      fft::fft_complex   *d_fft;
      std::vector<float> d_window;
      float              d_beta;

      std::vector<gr_complex> d_dup;
      std::vector<gr_complex> d_ddown;

      std::vector<gr_complex> d_upchirp;
      std::vector<gr_complex> d_downchirp;

      std::vector<gr_complex> d_dechirped;

      std::vector<unsigned short> d_symbols;

      std::ofstream f_up, f_down;

     public:
      demod_impl( float bandwidth,
                  unsigned short spreading_factor,
                  unsigned short code_rate,
                  float beta);
      ~demod_impl();

      unsigned short argmax(gr_complex *fft_result, bool update_squelch);

      // Where all the action really happens
      void forecast (int noutput_items, gr_vector_int &ninput_items_required);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);
    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_DEMOD_IMPL_H */

