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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "demod_impl.h"

namespace gr {
  namespace lora {

    demod::sptr
    demod::make(  int bandwidth,
                  short spreading_factor,
                  short code_rate)
    {
      return gnuradio::get_initial_sptr
        (new demod_impl(125000, 8, 4));
    }

    /*
     * The private constructor
     */
    demod_impl::demod_impl( int bandwidth,
                            short spreading_factor,
                            short code_rate)
      : gr::block("demod",
              // gr::io_signature::make(<+MIN_IN+>, <+MAX_IN+>, sizeof(gr_complex)),
              // gr::io_signature::make(<+MIN_OUT+>, <+MAX_OUT+>, sizeof(unsigned short)))
              gr::io_signature::make(1, 1, sizeof(gr_complex)),
              gr::io_signature::make(0, 1, sizeof(short)))
    {
      m_state = S_RESET;
      m_bw    = bandwidth;
      m_sf    = spreading_factor;
      m_cr    = code_rate;

      m_fft_size = (1 << spreading_factor);
      m_fft = new fft::fft_complex(m_fft_size, true, 1);

      memset(m_preamble_history, 0, PREAMBLE_HISTORY_DEPTH*sizeof(int));

      float phase = -M_PI;
      double accumulator = 0;
      for (int i = 0; i < m_fft_size; i++) {
        accumulator += phase;
        upchirp.push_back(gr_complex(std::conj(std::polar(1.0, accumulator))));
        downchirp.push_back(gr_complex(std::polar(1.0, accumulator)));
        phase += (2*M_PI)/m_fft_size;
      }

      set_history(LORA_HISTORY_DEPTH*pow(2, m_sf));  // Sync is 2.25 chirp periods long
    }

    /*
     * Our virtual destructor.
     */
    demod_impl::~demod_impl()
    {
    }

    short
    demod_impl::argmax(gr_complex *fft_result)
    {
      int max_idx = 0;
      float magsq = pow(real(fft_result[0]), 2) + pow(imag(fft_result[0]), 2);
      float max_val = magsq;


      for (int i = 0; i < m_fft_size; i++) {
        magsq = pow(real(fft_result[i]), 2) + pow(imag(fft_result[i]), 2);
        if (magsq > max_val)
        {
          max_idx = i;
          max_val = magsq;
        }
      }

      return max_idx;
    }

    void
    demod_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      // ninput_items_required[0] = pow(2, m_sf);
      ninput_items_required[0] = noutput_items * pow(2, m_sf);
    }

    int
    demod_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const gr_complex *in = (const gr_complex *) input_items[0];
      short *out = (short *) output_items[0];

      // Do <+signal processing+>
      // Tell runtime system how many input items we consumed on
      // each input stream.

      switch (m_state) {
      case S_RESET:
        memset(m_preamble_history, 0, PREAMBLE_HISTORY_DEPTH*sizeof(int));
        break;
      case S_DETECT_PREAMBLE:
        // fft.execute
        break;
      case S_DETECT_SYNC:
        break;
      case S_TUNE_SYNC:
        break;
      case S_READ_PAYLOAD:
        break;
      case S_OUT:
        break;
      default:
        break;
      }

      for(int i=0; i < m_fft_size; i++)
        std::cout << upchirp[i] << std::endl;
      while(1){;}


      consume_each (noutput_items);

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace lora */
} /* namespace gr */

