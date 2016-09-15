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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "demod_impl.h"

#define DEBUG_OFF     0
#define DEBUG_INFO    1
#define DEBUG_VERBOSE 2
#define DEBUG_ULTRA   3
#define DEBUG         DEBUG_OFF

#define OVERLAP_DEFAULT 1
#define OVERLAP_FACTOR  16

namespace gr {
  namespace lora {

    demod::sptr
    demod::make(  float bandwidth,
                  unsigned short spreading_factor,
                  unsigned short code_rate)
    {
      return gnuradio::get_initial_sptr
        (new demod_impl(bandwidth, spreading_factor, code_rate));
    }

    /*
     * The private constructor
     */
    demod_impl::demod_impl( float bandwidth,
                            unsigned short spreading_factor,
                            unsigned short code_rate)
      : gr::block("demod",
              gr::io_signature::make(1, 1, sizeof(gr_complex)),
              gr::io_signature::make(0, 0, 0)),
        f_up("up.out", std::ios::out),
        f_down("down.out", std::ios::out)
    {
      d_out_port = pmt::mp("out");

      message_port_register_out(d_out_port);

      d_state = S_RESET;
      d_bw    = bandwidth;
      d_sf    = spreading_factor;
      d_cr    = code_rate;

      d_fft_size = (1 << spreading_factor);
      d_fft = new fft::fft_complex(d_fft_size, true, 1);
      d_overlaps = OVERLAP_DEFAULT;
      d_offset = 0;

      d_power     = .000000001;     // MAGIC
      d_threshold = 0.0005;         // MAGIC

      for (int i = 0; i < d_fft_size; i++) {
        d_window.push_back(1.0);
      }
      // d_window[0]            = 0.0;
      // d_window[1]            = 0.68;
      // d_window[2]            = 0.95;
      // d_window[3]            = 0.98;
      // d_window[d_fft_size-4] = 0.98;
      // d_window[d_fft_size-3] = 0.95;
      // d_window[d_fft_size-2] = 0.68;
      // d_window[d_fft_size-1] = 0.0;
      d_window[0]            = 0.0;
      d_window[1]            = 0.6;
      d_window[2]            = 0.85;
      d_window[3]            = 0.93;
      d_window[4]            = 0.98;
      d_window[5]            = 0.99;
      d_window[d_fft_size-6] = 0.99;
      d_window[d_fft_size-5] = 0.98;
      d_window[d_fft_size-4] = 0.93;
      d_window[d_fft_size-3] = 0.85;
      d_window[d_fft_size-2] = 0.6;
      d_window[d_fft_size-1] = 0.0;

      float phase = -M_PI;
      double accumulator = 0;

      // Create local chirp tables.  Each table is 2 chirps long to allow memcpying from arbitrary offsets.
      for (int i = 0; i < 2*d_fft_size; i++) {
        accumulator += phase;
        d_downchirp.push_back(gr_complex(std::conj(std::polar(1.0, accumulator))));
        d_upchirp.push_back(gr_complex(std::polar(1.0, accumulator)));
        phase += (2*M_PI)/d_fft_size;
      }

      // f_up.write((const char*)&d_downchirp[0], d_fft_size*sizeof(gr_complex));
      // f_up.write((const char*)&d_upchirp[0], d_fft_size*sizeof(gr_complex));

      set_history(DEMOD_HISTORY_DEPTH*pow(2, d_sf));  // Sync is 2.25 chirp periods long
    }

    /*
     * Our virtual destructor.
     */
    demod_impl::~demod_impl()
    {
      delete d_fft;
    }

    unsigned short
    demod_impl::argmax(gr_complex *fft_result, 
                       bool update_squelch)
    {
      float magsq   = pow(real(fft_result[0]), 2) + pow(imag(fft_result[0]), 2);
      float max_val = magsq;
      unsigned short   max_idx = 0;


      for (unsigned short i = 0; i < d_fft_size; i++) {
        magsq = pow(real(fft_result[i]), 2) + pow(imag(fft_result[i]), 2);
        if (magsq > max_val)
        {
          max_idx = i;
          max_val = magsq;
        }
      }

      if (update_squelch) {
        d_power = max_val;
        d_squelched = (d_power > d_threshold) ? false : true;
      }

#if DEBUG >= DEBUG_ULTRA
      std::cout << "POWER " << d_power << " " << d_squelched << std::endl;
#endif

      return max_idx;
    }

    void
    demod_impl::forecast (int noutput_items,
                          gr_vector_int &ninput_items_required)
    {
      // ninput_items_required[0] = pow(2, d_sf);
      ninput_items_required[0] = noutput_items * pow(2, d_sf);
    }

    int
    demod_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const gr_complex *in = (const gr_complex *)  input_items[0];
      unsigned short  *out = (unsigned short   *) output_items[0];
      unsigned short num_consumed = d_fft_size;
      unsigned short max_index = 0;
      bool preamble_found = false;
      bool sfd_found      = false;

      // Nomenclature:
      //  up_block   == de-chirping buffer to contain upchirp features: the preamble, sync word, and data chirps
      //  down_block == de-chirping buffer to contain downchirp features: the SFD
      gr_complex *buffer     = (gr_complex *)volk_malloc(d_fft_size*sizeof(gr_complex), volk_get_alignment());
      gr_complex *up_block   = (gr_complex *)volk_malloc(d_fft_size*sizeof(gr_complex), volk_get_alignment());
      gr_complex *down_block = (gr_complex *)volk_malloc(d_fft_size*sizeof(gr_complex), volk_get_alignment());

      if (buffer == NULL || up_block == NULL || down_block == NULL)
      {
        std::cerr << "Unable to allocate processing buffer!" << std::endl;
      }

      // Dechirp the incoming signal
      volk_32fc_x2_multiply_32fc(  down_block, in, &d_upchirp[0], d_fft_size);
      volk_32fc_x2_multiply_32fc(up_block, in, &d_downchirp[d_offset], d_fft_size);

      // Windowing
      // volk_32fc_32f_multiply_32fc(up_block, up_block, &d_window[0], d_fft_size);

      if (d_state == S_READ_PAYLOAD)
      {
        f_up.write  ((const char*)&down_block[0]  , d_fft_size*sizeof(gr_complex));
        f_down.write((const char*)&up_block[0], d_fft_size*sizeof(gr_complex));
      }

      // Preamble and Data FFT
      memset(d_fft->get_inbuf(),              0, d_fft_size*sizeof(gr_complex));
      memcpy(d_fft->get_inbuf(), &up_block[0], d_fft_size*sizeof(gr_complex));
      d_fft->execute();

      // Take argmax of returned FFT (similar to MFSK demod)
      max_index = argmax(d_fft->get_outbuf(), true);
      d_argmax_history.insert(d_argmax_history.begin(), max_index);

      if (d_argmax_history.size() > REQUIRED_PREAMBLE_DEPTH)
      {
        d_argmax_history.pop_back();
      }

      switch (d_state) {
      case S_RESET:
        d_overlaps = OVERLAP_DEFAULT;
        d_offset = 0;
        d_symbols.clear();
        d_argmax_history.clear();
        d_sfd_history.clear();

        d_state = S_PREFILL;

        #if DEBUG >= DEBUG_INFO
          std::cout << "Next state: S_PREFILL" << std::endl;
        #endif

        break;



      case S_PREFILL:
        if (d_argmax_history.size() >= REQUIRED_PREAMBLE_DEPTH)
        {
          d_state = S_DETECT_PREAMBLE;

          #if DEBUG >= DEBUG_INFO
            std::cout << "Next state: S_DETECT_PREAMBLE" << std::endl;
          #endif
        }
        break;



      // Looks for the same symbol appearing consecutively, signifying the LoRa preamble
      case S_DETECT_PREAMBLE:
        d_preamble_idx = d_argmax_history[0];

        #if DEBUG >= DEBUG_VERBOSE
          std::cout << "PREAMBLE " << d_argmax_history[0] << std::endl;
        #endif

        for (int i = 1; i < REQUIRED_PREAMBLE_DEPTH; i++)
        {
          preamble_found = true;
          if (d_preamble_idx != d_argmax_history[i])
          {
            preamble_found = false;
          }
        }

        if (preamble_found and !d_squelched)   // TODO and squelch open
        {
          d_state = S_SFD_SYNC;   // TODO correct state

          #if DEBUG >= DEBUG_INFO
            std::cout << "Next state: S_SFD_SYNC" << std::endl;
          #endif
        }
        break;



      // Accurately synchronize to the SFD by computing overlapping FFTs of the downchirp/SFD IQ stream
      // Effectively increases FFT's time-based resolution, allowing for a better sync
      case S_SFD_SYNC:
        d_overlaps = OVERLAP_FACTOR;

        for (int ol = 0; ol < d_overlaps; ol++)
        {
          d_offset = ((ol*d_fft_size)/d_overlaps) % d_fft_size;

          for (int j = 0; j < d_fft_size; j++)
          {
            buffer[j] = in[d_offset+j];
          }

          #if DEBUG >= DEBUG_VERBOSE
            std::cout << "ol: " << std::dec << ol << " d_overlaps: " << d_overlaps << std::endl;
          #endif

          // volk_32fc_x2_multiply_32fc(  down_block, buffer, &d_upchirp[0], d_fft_size);
          volk_32fc_x2_multiply_32fc(  down_block, buffer, &d_upchirp[d_offset], d_fft_size);

          // std::cout << "FFT WINDOW START " << offset << std::endl;
          memset(d_fft->get_inbuf(),        0, d_fft_size*sizeof(gr_complex));
          memcpy(d_fft->get_inbuf(), down_block, d_fft_size*sizeof(gr_complex));
          d_fft->execute();

          f_up.write  ((const char*)&down_block[0]  , d_fft_size*sizeof(gr_complex));

          // Take argmax of returned FFT (similar to MFSK demod)
          max_index = argmax(d_fft->get_outbuf(), false);
          d_sfd_history.insert(d_sfd_history.begin(), max_index);

          if (d_sfd_history.size() > REQUIRED_SFD_CHIRPS*OVERLAP_FACTOR)
          {
            d_sfd_history.pop_back();

            sfd_found = true;
            d_sfd_idx = d_sfd_history[0];

            for (int i = 1; i < REQUIRED_SFD_CHIRPS*OVERLAP_FACTOR; i++)
            {
              if (abs(short(d_sfd_idx) - short(d_sfd_history[i])) >= LORA_SFD_TOLERANCE)
              {
                sfd_found = false;
              }
            }

            if (sfd_found) {
              num_consumed = (ol*d_fft_size)/d_overlaps + 5*d_fft_size/4;   // Skip last quarter chirp
              // d_preamble_idx = (d_preamble_idx + num_consumed) % d_fft_size;

              d_offset = (d_offset + (d_fft_size/4)) % d_fft_size;   // @MKNIGHT

              d_state = S_READ_PAYLOAD;
              d_overlaps = OVERLAP_DEFAULT;

              #if DEBUG >= DEBUG_INFO
                std::cout << "Next state: S_READ_PAYLOAD" << std::endl;
              #endif

              break;
            }
          }
        }

        break;



      case S_READ_PAYLOAD:
        if (d_squelched)
        {
          d_state = S_OUT;

          #if DEBUG >= DEBUG_INFO
            std::cout << "Next state: S_OUT" << std::endl;
          #endif
        }

        d_symbols.push_back((d_argmax_history[0]-d_preamble_idx+d_fft_size) % d_fft_size);

        break;



      case S_OUT:
      {
        pmt::pmt_t output = pmt::init_u16vector(d_symbols.size(), d_symbols);
        pmt::pmt_t msg_pair = pmt::cons(pmt::make_dict(), output);
        message_port_pub(d_out_port, msg_pair);

        d_state = S_RESET;

        #if DEBUG >= DEBUG_INFO
          std::cout << "Next state: S_RESET" << std::endl;
        #endif

        break;
      }



      default:
        break;
      }

      consume_each (num_consumed);

      free(down_block);
      free(up_block);
      free(buffer);

      return noutput_items;
    }

  } /* namespace lora */
} /* namespace gr */

