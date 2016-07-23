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
              gr::io_signature::make(0, 1, sizeof(short))),
      f_up("chirp.out", std::ios::out),
      f_down("down.out", std::ios::out)
    {
      m_state = S_RESET;
      m_bw    = bandwidth;
      m_sf    = spreading_factor;
      m_cr    = code_rate;

      m_fft_size = (1 << spreading_factor);
      m_fft = new fft::fft_complex(m_fft_size, true, 1);

      for (int i = 0; i < m_fft_size; i++) {
        m_window.push_back(1.0);
      }

      // memset(m_argmax_history, 0, REQUIRED_PREAMBLE_DEPTH*sizeof(short));

      float phase = -M_PI;
      // float phase = 0;
      double accumulator = 0;

      for (int i = 0; i < m_fft_size; i++) {
        accumulator += phase;
        m_upchirp.push_back(gr_complex(std::conj(std::polar(1.0, accumulator))));
        m_downchirp.push_back(gr_complex(std::polar(1.0, accumulator)));
        phase += (2*M_PI)/m_fft_size;
      }

      // f_up.write((const char*)&m_upchirp[0], m_fft_size*sizeof(gr_complex));
      // f_up.write((const char*)&m_downchirp[0], m_fft_size*sizeof(gr_complex));

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
    demod_impl::dechirp (const gr_complex *inbuf, std::vector<gr_complex>& chirp, std::vector<gr_complex>& dechirped)
    {
      std::cout << "1" << std::endl;
      // for (int i = 0; i < m_fft_size; i++)
      // {
      //   std::cout << "." << i << std::endl;
      //   std::cout << inbuf[i] << std::endl;
      //   std::cout << chirp[i] << std::endl;
      //   // dechirped[i] = inbuf[i] * chirp[i];
      //   volk_32fc_x2_multiply_32fc(&dechirped[i], &inbuf[i], &chirp[i], sizeof(gr_complex));

      //   std::cout << "." << std::endl;
      // }
      // volk_32fc_x2_multiply_32fc(&dechirped, &inbuf, &chirp, m_fft_size*sizeof(gr_complex));
      std::cout << "2" << std::endl;
      memcpy(m_fft->get_inbuf(), &dechirped[0], m_fft_size*sizeof(gr_complex));
      std::cout << "3" << std::endl;
      m_fft->execute();
      std::cout << "4" << std::endl;
      memcpy(&dechirped[0], m_fft->get_outbuf(), m_fft_size*sizeof(gr_complex));
      std::cout << "5" << std::endl;
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

      short max_index = 0;
      short preamble_idx = 0;
      bool preamble_found = false;
      gr_complex *processing = (gr_complex *)malloc(m_fft_size*sizeof(gr_complex));

      if (processing == NULL)
      {
        std::cerr << "Unable to allocate processing buffer!" << std::endl;
      }

      // Do <+signal processing+>
      // Tell runtime system how many input items we consumed on
      // each input stream.

      // Dechirp the incoming signal
      for (int i = 0; i < m_fft_size; i++)
      {
        volk_32fc_32f_multiply_32fc(&processing[i], &in[i], (float *)&m_downchirp[i], 1);
      }

      // FFT
      memcpy(m_fft->get_inbuf(), &processing[0], m_fft_size*sizeof(gr_complex));
      m_fft->execute();

      // Take argmax of returned FFT (similar to MFSK demod)
      max_index = argmax(m_fft->get_outbuf());
      m_argmax_history.insert(m_argmax_history.begin(), max_index);
      if (m_argmax_history.size() > REQUIRED_PREAMBLE_DEPTH)
      {
        m_argmax_history.pop_back();
        f_up.write((const char*)&m_argmax_history[0], sizeof(short));
      }

      switch (m_state) {
      case S_RESET:
        if (m_argmax_history.size() >= REQUIRED_PREAMBLE_DEPTH)
        {
          m_state = S_DETECT_PREAMBLE;
          std::cout << "New state: S_DETECT_PREAMBLE" << std::endl;
        }
        break;

      case S_DETECT_PREAMBLE:
        preamble_found = true;
        preamble_idx = m_argmax_history[0];

        for (int i = 1; i < REQUIRED_PREAMBLE_DEPTH; i++)
        {
          if (preamble_idx != m_argmax_history[i])
          {
            preamble_found = false;
          }
        }

        if (preamble_found)   // TODO and squelch open
        {
          m_state = S_DETECT_SYNC;
          std::cout << "New state: S_DETECT_SYNC" << std::endl;
        }
        break;

      case S_DETECT_SYNC:
        f_down.write((const char*)&m_argmax_history[0], sizeof(short));
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

      // for(int i=0; i < m_fft_size; i++)
      //   std::cout << upchirp[i] << std::endl;

      // f_up.write((const char*)&m_dup[0], m_fft_size*sizeof(gr_complex));

      // while(1){;}

      consume_each (m_fft_size);

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace lora */
} /* namespace gr */

