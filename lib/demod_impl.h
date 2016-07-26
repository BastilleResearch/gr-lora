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

#ifndef INCLUDED_LORA_DEMOD_IMPL_H
#define INCLUDED_LORA_DEMOD_IMPL_H

#include <cmath>
#include <cstdlib>
#include <vector>
#include <queue>
#include <complex>
#include <fstream>
#include <gnuradio/fft/fft.h>
#include <volk/volk.h>
#include "lora/demod.h"

namespace gr {
  namespace lora {

    class demod_impl : public demod
    {
     private:
      demod_state_t   m_state;
      int             m_bw;
      unsigned short  m_sf;
      unsigned short  m_cr;

      unsigned short  m_fft_size;
      unsigned short  m_overlaps;

      float           m_power;
      float           m_threshold;
      bool            m_squelched;

      unsigned short  m_preamble_idx;
      unsigned short  m_sfd_idx;
      std::vector<unsigned short>  m_argmax_history;
      std::vector<unsigned short>  m_sfd_history;

      fft::fft_complex   *m_fft;
      std::vector<float> m_window;

      std::vector<gr_complex> m_dup;
      std::vector<gr_complex> m_ddown;

      std::vector<gr_complex> m_upchirp;
      std::vector<gr_complex> m_downchirp;

      std::vector<gr_complex> m_dechirped;

      std::vector<unsigned short> m_symbols;

      std::ofstream f_up, f_down;

     public:
      demod_impl( int bandwidth,
                  unsigned short spreading_factor,
                  unsigned short code_rate);
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

