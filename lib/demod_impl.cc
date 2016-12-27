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
#define DEBUG         DEBUG_OFF

#define DUMP_IQ       0

#define OVERLAP_DEFAULT 1
#define OVERLAP_FACTOR  16

namespace gr {
  namespace lora {

    demod::sptr
    demod::make(  unsigned short spreading_factor,
                  bool  low_data_rate,
                  float beta,
                  unsigned short fft_factor)
    {
      return gnuradio::get_initial_sptr
        (new demod_impl(spreading_factor, low_data_rate, beta, fft_factor));
    }

    /*
     * The private constructor
     */
    demod_impl::demod_impl( unsigned short spreading_factor,
                            bool  low_data_rate,
                            float beta,
                            unsigned short fft_factor)
      : gr::block("demod",
              gr::io_signature::make(1, 1, sizeof(gr_complex)),
              gr::io_signature::make(0, 0, 0)),
        f_raw("raw.out", std::ios::out),
        f_up_windowless("up_windowless.out", std::ios::out),
        f_up("up.out", std::ios::out),
        f_down("down.out", std::ios::out),
        d_sf(spreading_factor),
        d_ldr(low_data_rate),
        d_beta(beta),
        d_fft_size_factor(fft_factor)
    {
      assert((d_sf > 5) && (d_sf < 13));
      if (d_sf == 6) assert(!header);
      assert(d_fft_size_factor > 0);

      d_out_port = pmt::mp("out");
      message_port_register_out(d_out_port);

      d_state = S_RESET;

      d_num_symbols = (1 << d_sf);
      d_fft_size = d_fft_size_factor*d_num_symbols;
      d_fft = new fft::fft_complex(d_fft_size, true, 1);
      d_overlaps = OVERLAP_DEFAULT;
      d_offset = 0;

      d_window = fft::window::build(fft::window::WIN_KAISER, d_num_symbols, d_beta);

      d_power     = .000000001;     // MAGIC
      d_threshold = 0.005;          // MAGIC
      // d_threshold = 0.003;          // MAGIC
      // d_threshold = 0.12;           // MAGIC

      float phase = -M_PI;
      double accumulator = 0;

      // Create local chirp tables.  Each table is 2 chirps long to allow memcpying from arbitrary offsets.
      for (int i = 0; i < 2*d_num_symbols; i++) {
        accumulator += phase;
        d_downchirp.push_back(gr_complex(std::conj(std::polar(1.0, accumulator))));
        d_upchirp.push_back(gr_complex(std::polar(1.0, accumulator)));
        phase += (2*M_PI)/d_num_symbols;
      }

      set_history(DEMOD_HISTORY_DEPTH*d_num_symbols);  // Sync is 2.25 chirp periods long
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


      for (unsigned short i = 0; i < d_fft_size; i++)
      {
        magsq = pow(real(fft_result[i]), 2) + pow(imag(fft_result[i]), 2);
        if (magsq > max_val)
        {
          max_idx = i;
          max_val = magsq;
        }
      }

      if (update_squelch)
      {
        d_power = max_val;
        d_squelched = (d_power > d_threshold) ? false : true;
      }

      return max_idx;
    }

    void
    demod_impl::forecast (int noutput_items,
                          gr_vector_int &ninput_items_required)
    {
      ninput_items_required[0] = noutput_items * (1 << d_sf);
    }

    int
    demod_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const gr_complex *in = (const gr_complex *)  input_items[0];
      unsigned short  *out = (unsigned short   *) output_items[0];
      unsigned short num_consumed = d_num_symbols;
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
      volk_32fc_x2_multiply_32fc(down_block, in, &d_upchirp[0], d_num_symbols);

      if (d_state == S_READ_HEADER || d_state == S_READ_PAYLOAD)
      {
        volk_32fc_x2_multiply_32fc(up_block, in, &d_downchirp[d_offset], d_num_symbols);
      }
      else
      {
        volk_32fc_x2_multiply_32fc(up_block, in, &d_downchirp[0], d_num_symbols);
      }

      // Enable to write IQ to disk for debugging
      #if DUMP_IQ
        f_up_windowless.write((const char*)&up_block[0], d_num_symbols*sizeof(gr_complex));
      #endif

      // Windowing
      volk_32fc_32f_multiply_32fc(up_block, up_block, &d_window[0], d_num_symbols);

      #if DUMP_IQ
        if (d_state != S_SFD_SYNC) f_down.write((const char*)&down_block[0], d_num_symbols*sizeof(gr_complex));
        f_up.write((const char*)&up_block[0], d_num_symbols*sizeof(gr_complex));
      #endif

      // Preamble and Data FFT
      // If d_fft_size_factor is greater than 1, the rest of the sample buffer will be zeroed out and blend into the window
      memset(d_fft->get_inbuf(),            0, d_fft_size*sizeof(gr_complex));
      memcpy(d_fft->get_inbuf(), &up_block[0], d_num_symbols*sizeof(gr_complex));
      d_fft->execute();

      // Take argmax of returned FFT (similar to MFSK demod)
      max_index = argmax(d_fft->get_outbuf(), true);
      d_argmax_history.insert(d_argmax_history.begin(), max_index);

      if (d_argmax_history.size() > REQUIRED_PREAMBLE_CHIRPS)
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
        d_sync_recovery_counter = 0;

        d_state = S_PREFILL;

        #if DEBUG >= DEBUG_INFO
          std::cout << "Next state: S_PREFILL" << std::endl;
        #endif

        break;



      case S_PREFILL:
        if (d_argmax_history.size() >= REQUIRED_PREAMBLE_CHIRPS)
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

        // Check for discontinuities that exceed some tolerance
        preamble_found = true;
        for (int i = 1; i < REQUIRED_PREAMBLE_CHIRPS; i++)
        {
          if (abs(int(d_preamble_idx) - int(d_argmax_history[i])) > LORA_PREAMBLE_TOLERANCE)
          {
            preamble_found = false;
          }
        }

        // Advance to SFD/sync discovery if a contiguous preamble is found
        if (preamble_found and !d_squelched)
        {
          d_state = S_SFD_SYNC;

          #if DEBUG >= DEBUG_INFO
            std::cout << "Next state: S_SFD_SYNC" << std::endl;
          #endif
        }
        break;



      // Accurately synchronize to the SFD by computing overlapping FFTs of the downchirp/SFD IQ stream
      // Effectively increases FFT's time-based resolution, allowing for a better sync
      case S_SFD_SYNC:
        d_overlaps = OVERLAP_FACTOR;

        // Recover if the SFD is missed, or if we wind up in this state erroneously (false positive on preamble)
        if (d_sync_recovery_counter++ > DEMOD_SYNC_RECOVERY_COUNT)
        {
          d_state = S_RESET;
          d_overlaps = OVERLAP_DEFAULT;

          #if DEBUG >= DEBUG_INFO
            std::cout << "Bailing out of sync loop"   << std::endl;
            std::cout << "Next state: S_RESET" << std::endl;
          #endif
        }

        // Iterate through sample buffer
        for (int ol = 0; ol < d_overlaps; ol++)
        {
          d_offset = ((ol*d_num_symbols)/d_overlaps) % d_num_symbols;

          // Fill working buffer based on offset
          for (int j = 0; j < d_num_symbols; j++)
          {
            buffer[j] = in[d_offset+j];
          }

          #if DEBUG >= DEBUG_VERBOSE
            std::cout << "ol: " << std::dec << ol << " d_overlaps: " << d_overlaps << std::endl;
          #endif

          // Dechirp
          volk_32fc_x2_multiply_32fc(down_block, buffer, &d_upchirp[d_offset], d_num_symbols);

          // Enable to write out overlapped chirps to disk for debugging
          #if DUMP_IQ
            f_down.write((const char*)&down_block[0], d_num_symbols*sizeof(gr_complex));
          #endif

          memset(d_fft->get_inbuf(),          0, d_fft_size*sizeof(gr_complex));
          memcpy(d_fft->get_inbuf(), down_block, d_num_symbols*sizeof(gr_complex));
          d_fft->execute();

          // Take argmax of downchirp FFT
          max_index = argmax(d_fft->get_outbuf(), false); 
          d_sfd_history.insert(d_sfd_history.begin(), max_index);

          if (d_sfd_history.size() > REQUIRED_SFD_CHIRPS*OVERLAP_FACTOR)
          {
            d_sfd_history.pop_back();

            sfd_found = true;
            d_sfd_idx = d_sfd_history[0];

            // Check for discontinuities that exceed some preset tolerance
            for (int i = 1; i < REQUIRED_SFD_CHIRPS*OVERLAP_FACTOR; i++)
            {
              if (abs(int(d_sfd_idx) - int(d_sfd_history[i])) > d_fft_size_factor*LORA_SFD_TOLERANCE)
              {
                sfd_found = false;
              }
            }

            // If within tolerance, we've found the SFD and are synchronized
            if (sfd_found)
            {
              num_consumed = (ol*d_num_symbols)/d_overlaps + 5*d_num_symbols/4;   // Skip last quarter chirp
              d_offset = (d_offset + (d_num_symbols/4)) % d_num_symbols;

              d_state = S_READ_HEADER;
              d_overlaps = OVERLAP_DEFAULT;

              #if DEBUG >= DEBUG_INFO
                std::cout << "Next state: S_READ_HEADER" << std::endl;
              #endif

              break;
            }
          }
        }

        break;



      case S_READ_HEADER:
        if (d_squelched)
        {
          d_state = S_OUT;

          #if DEBUG >= DEBUG_INFO
            std::cout << "Next state: S_OUT" << std::endl;
          #endif
        }
        else if (d_symbols.size() == 7)   // Symbols [0:7] contain 2**(SF-2) bits/symbol, symbols [8:] have the full 2**(SF) bits
        {
          d_state = S_READ_PAYLOAD;

          #if DEBUG >= DEBUG_INFO
            std::cout << "Next state: S_READ_PAYLOAD" << std::endl;
          #endif
        }

        /* Preamble + modulo operation normalizes the symbols about the preamble; preamble symbol == value 0
         * Dividing by d_fft_size_factor reduces symbols to [0:(2**sf)-1] range
         * Dividing by 4 to further reduce symbol set to [0:(2**(sf-2)-1)], since header is sent at SF-2
         */
        d_symbols.push_back( ( ( d_num_symbols + (d_argmax_history[0]/d_fft_size_factor) - (d_preamble_idx/d_fft_size_factor)) % d_num_symbols) / 4);

        break;



      case S_READ_PAYLOAD:
        if (d_squelched)
        {
          d_state = S_OUT;

          #if DEBUG >= DEBUG_INFO
            std::cout << "Next state: S_OUT" << std::endl;
          #endif
        }

        /* Preamble + modulo operation normalizes the symbols about the preamble; preamble symbol == value 0
         * Dividing by d_fft_size_factor reduces symbols to [0:(2**sf)-1] range
         */
        if (d_ldr)  // if low data rate optimization is on, give entire packet the header treatment of ppm == SF-2
        {
          d_symbols.push_back( ( ( d_num_symbols + (d_argmax_history[0]/d_fft_size_factor) - (d_preamble_idx/d_fft_size_factor)) % d_num_symbols) / 4);
        }
        else
        {
          d_symbols.push_back( ( d_num_symbols + (d_argmax_history[0]/d_fft_size_factor) - (d_preamble_idx/d_fft_size_factor)) % d_num_symbols);  
        }

        break;



      // Emit a PDU to the decoder
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

      #if DUMP_IQ
        f_raw.write((const char*)&in[0], num_consumed*sizeof(gr_complex));
      #endif

      consume_each (num_consumed);

      free(down_block);
      free(up_block);
      free(buffer);

      return noutput_items;
    }

  } /* namespace lora */
} /* namespace gr */

