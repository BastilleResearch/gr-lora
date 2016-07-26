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

#define DEBUG_OFF     0
#define DEBUG_INFO    1
#define DEBUG_VERBOSE 2
#define DEBUG_ULTRA   3
#define DEBUG         DEBUG_VERBOSE

#define OVERLAP_DEFAULT 1
#define OVERLAP_FACTOR  8

namespace gr {
  namespace lora {

    demod::sptr
    demod::make(  int bandwidth,
                  unsigned short spreading_factor,
                  unsigned short code_rate)
    {
      return gnuradio::get_initial_sptr
        (new demod_impl(125000, 8, 4));
    }

    /*
     * The private constructor
     */
    demod_impl::demod_impl( int bandwidth,
                            unsigned short spreading_factor,
                            unsigned short code_rate)
      : gr::block("demod",
              gr::io_signature::make(1, 1, sizeof(gr_complex)),
              gr::io_signature::make(0, 1, sizeof(unsigned short))),
      f_up("up.out", std::ios::out),
      f_down("down.out", std::ios::out)
    {
      m_state = S_RESET;
      m_bw    = bandwidth;
      m_sf    = spreading_factor;
      m_cr    = code_rate;

      m_fft_size = (1 << spreading_factor);
      m_fft = new fft::fft_complex(m_fft_size, true, 1);
      m_overlaps = OVERLAP_DEFAULT;

      m_power     = .000000001;     // MAGIC
      m_threshold = 0.0005;         // MAGIC

      for (int i = 0; i < m_fft_size; i++) {
        m_window.push_back(1.0);
      }
      m_window[0]            = 0.0;
      m_window[1]            = 0.68;
      m_window[2]            = 0.95;
      m_window[3]            = 0.98;
      m_window[m_fft_size-4] = 0.98;
      m_window[m_fft_size-3] = 0.95;
      m_window[m_fft_size-2] = 0.68;
      m_window[m_fft_size-1] = 0.0;

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
      delete m_fft;
    }

    unsigned short
    demod_impl::argmax(gr_complex *fft_result, bool update_squelch)
    {
      float magsq   = pow(real(fft_result[0]), 2) + pow(imag(fft_result[0]), 2);
      float max_val = magsq;
      unsigned short   max_idx = 0;


      for (unsigned short i = 0; i < m_fft_size; i++) {
        magsq = pow(real(fft_result[i]), 2) + pow(imag(fft_result[i]), 2);
        if (magsq > max_val)
        {
          max_idx = i;
          max_val = magsq;
        }
      }

      if (update_squelch) {
        m_power = max_val;
        m_squelched = (m_power > m_threshold) ? false : true;
      }

#if DEBUG >= DEBUG_ULTRA
      std::cout << "POWER " << m_power << " " << m_squelched << std::endl;
#endif

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
      unsigned short *out = (unsigned short *) output_items[0];

      unsigned short max_index = 0;
      bool preamble_found = false;
      bool sfd_found      = false;
      gr_complex *up_block = (gr_complex *)malloc(m_fft_size*sizeof(gr_complex));
      gr_complex *down_block = (gr_complex *)malloc(m_fft_size*sizeof(gr_complex));

      if (up_block == NULL || down_block == NULL)
      {
        std::cerr << "Unable to allocate processing buffer!" << std::endl;
      }

      // Do <+signal processing+>
      // Tell runtime system how many input items we consumed on
      // each input stream.

      noutput_items = 0;

      // Dechirp the incoming signal
      volk_32fc_x2_multiply_32fc(up_block, in, &m_downchirp[0], m_fft_size);
      volk_32fc_x2_multiply_32fc(down_block, in, &m_upchirp[0], m_fft_size);

      f_up.write((const char*)&up_block[0], m_fft_size*sizeof(gr_complex));
      f_down.write((const char*)&down_block[0], m_fft_size*sizeof(gr_complex));

      // Preamble and Data FFT
      memcpy(m_fft->get_inbuf(), &down_block[0], m_fft_size*sizeof(gr_complex));
      m_fft->execute();
      // Take argmax of returned FFT (similar to MFSK demod)
      max_index = argmax(m_fft->get_outbuf(), true);
      m_argmax_history.insert(m_argmax_history.begin(), max_index);

      if (m_argmax_history.size() > REQUIRED_PREAMBLE_DEPTH)
      {
        m_argmax_history.pop_back();
      }

      // SFD Downchirp FFT
      memcpy(m_fft->get_inbuf(), &up_block[0], m_fft_size*sizeof(gr_complex));
      m_fft->execute();
      // Take argmax of returned FFT (similar to MFSK demod)
      max_index = argmax(m_fft->get_outbuf(), false);
      m_sfd_history.insert(m_sfd_history.begin(), max_index);

      if (m_sfd_history.size() > REQUIRED_SFD_CHIRPS*OVERLAP_FACTOR)
      {
        m_sfd_history.pop_back();
      }

      switch (m_state) {
      case S_RESET:
        m_overlaps = OVERLAP_DEFAULT;
        m_symbols.clear();

        if (m_argmax_history.size() >= REQUIRED_PREAMBLE_DEPTH)
        {
          m_state = S_DETECT_PREAMBLE;
#if DEBUG >= DEBUG_INFO
          std::cout << "New state: S_DETECT_PREAMBLE" << std::endl;
#endif
        }
        break;



      case S_DETECT_PREAMBLE:
        m_preamble_idx = m_argmax_history[0];

#if DEBUG >= DEBUG_VERBOSE
        std::cout << "PREAMBLE " << m_argmax_history[0] << std::endl;
#endif

        for (int i = 1; i < REQUIRED_PREAMBLE_DEPTH; i++)
        {
          preamble_found = true;
          if (m_preamble_idx != m_argmax_history[i])
          {
            preamble_found = false;
          }
        }

        if (preamble_found and !m_squelched)   // TODO and squelch open
        {
          m_state = S_READ_PAYLOAD;   // TODO correct state
#if DEBUG >= DEBUG_INFO
          std::cout << "New state: S_DETECT_SYNC" << std::endl;
#endif
        }
        break;



      case S_DETECT_SYNC:
//         m_overlaps = 8;     // MAGIC
//         m_sfd_idx = m_sfd_history[0];

// #if DEBUG >= DEBUG_VERBOSE
//         std::cout <<"SFD " << m_sfd_history[0] << " SFD SIZE " << m_sfd_history.size() << std::endl;
// #endif

//         if (m_sfd_history.size() >= REQUIRED_SFD_CHIRPS*m_overlaps)
//         {
//           sfd_found = true;
//           for (int i = 1; i < REQUIRED_SFD_CHIRPS*m_overlaps; i++)
//           {
//             if (m_sfd_idx != m_sfd_history[i])
//             {
//               sfd_found = false;
//             }
//           }

//           if (sfd_found)
//           {
//             m_state = S_READ_PAYLOAD;
//   #if DEBUG >= DEBUG_INFO
//             std::cout << "New state: S_READ_PAYLOAD" << std::endl;
//   #endif
//           }
//         }

        break;



      case S_TUNE_SYNC:
        break;



      case S_READ_PAYLOAD:
        m_symbols.push_back((m_argmax_history[0]-m_preamble_idx+m_fft_size) % m_fft_size);
        if (m_symbols.size() >= 8) m_state = S_OUT;
        break;



      case S_OUT:
        if (m_squelched)
        {
          m_state = S_RESET;
          *out = (unsigned short)m_fft_size;
          noutput_items = 1;
        }
        else
        {
#if DEBUG >= DEBUG_VERBOSE
          std::cout << "LORA DEMOD SYMBOL   " << (unsigned short)((m_argmax_history[0] - m_preamble_idx+m_fft_size) % m_fft_size) << std::endl;
#endif
          m_symbols.push_back((m_argmax_history[0]-m_preamble_idx+m_fft_size) % m_fft_size);

          *out = (unsigned short)((m_argmax_history[0]-m_preamble_idx+m_fft_size) % m_fft_size);
          noutput_items = 1;
        }

        // std::cout << "Received frame:" << std::endl;
        // for (int i = 8; i < m_symbols.size(); i++)
        // {
        //   std::cout << "OUT SYMBOL " << m_symbols[i] << std::endl;
        // }

        // noutput_items = m_symbols.size();
        // m_state = S_RESET;

        break;



      default:
        break;
      }

      consume_each (m_fft_size/m_overlaps);

      free(up_block);
      free(down_block);

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace lora */
} /* namespace gr */

