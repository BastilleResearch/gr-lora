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
//#include <volk/volk.h>
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
      f_chirp("chirp.out", std::ios::out)
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

      memset(m_preamble_history, 0, PREAMBLE_HISTORY_DEPTH*sizeof(int));

      float phase = -M_PI;
      // float phase = 0;
      double accumulator = 0;

      for (int i = 0; i < m_fft_size; i++) {
        accumulator += phase;
        m_upchirp.push_back(gr_complex(std::conj(std::polar(1.0, accumulator))));
        m_downchirp.push_back(gr_complex(std::polar(1.0, accumulator)));
        phase += (2*M_PI)/m_fft_size;

        // f_chirp.write((const char*)&m_upchirp[i], sizeof(gr_complex));
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

      std::cout << "ninput_items" << ninput_items[0] << std::endl;
      std::cout << "noutput_items" << noutput_items << std::endl;

      // Do <+signal processing+>
      // Tell runtime system how many input items we consumed on
      // each input stream.

      // De-chirp the incoming signal
      // for (int i = 0; i < ninput_items[0]; i++)
      // {
      //   volk_32fc_32f_multiply_32fc(&upchirp[i], &in[i], &m_dup[i], sizeof(gr_complex));
      //   volk_32fc_32f_multiply_32fc(&downchirp[i], &in[i], &m_ddown[i], sizeof(gr_complex));
      // }
      // memcpy(m_fft->get_inbuf(), &m_dup[0], m_fft_size*sizeof(gr_complex));
      // m_fft->execute();

      gr_complex *data = (gr_complex *)malloc(ninput_items[0]*sizeof(gr_complex));

      for (int i = 0; i < ninput_items[0]; i++)
      {
        volk_32fc_32f_multiply_32fc(&data[i], &in[i], (float *)&m_downchirp[i%m_fft_size], 1);
      }

      std::cout << "0" << std::endl;
      // f_chirp.write((const char*)m_fft->get_outbuf(), m_fft_size*sizeof(gr_complex));
      f_chirp.write((const char*)&data[0], ninput_items[0]*sizeof(gr_complex));

      free(data);

      // if (ninput_items[0] >= m_fft_size)
      // {
      //   dechirp(in, m_upchirp, m_dup);
      //   std::cout << "6" << std::endl;
      //   dechirp(in, m_downchirp, m_ddown);
      // // }

      switch (m_state) {
      case S_RESET:
        memset(m_preamble_history, 0, PREAMBLE_HISTORY_DEPTH*sizeof(int));
        break;
      case S_DETECT_PREAMBLE:

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

      // for(int i=0; i < m_fft_size; i++)
      //   std::cout << upchirp[i] << std::endl;

      // f_chirp.write((const char*)&m_dup[0], m_fft_size*sizeof(gr_complex));
      std::cout << "I am here." << std::endl;

      // while(1){;}

      consume_each (noutput_items);

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace lora */
} /* namespace gr */

