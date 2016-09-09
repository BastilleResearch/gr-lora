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
#include <volk/volk.h>
#include "mod_impl.h"

namespace gr {
  namespace lora {

    mod::sptr
    mod::make(  int bandwidth,
                short spreading_factor,
                short code_rate)
    {
      return gnuradio::get_initial_sptr
        (new mod_impl(bandwidth, spreading_factor, code_rate));
    }

    /*
     * The private constructor
     */
    mod_impl::mod_impl( int bandwidth,
                        short spreading_factor,
                        short code_rate)
      : gr::block("mod",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0)),
        f_mod("mod.out", std::ios::out)
    {
      d_in_port = pmt::mp("in");
      d_out_port = pmt::mp("out");

      message_port_register_in(d_in_port);
      message_port_register_out(d_out_port);

      set_msg_handler(d_in_port, boost::bind(&mod_impl::modulate, this, _1));

      d_sf = spreading_factor;
      d_cr = code_rate;

      d_fft_size = (1 << spreading_factor);

      float phase = -M_PI;
      double accumulator = 0;

      for (int i = 0; i < d_fft_size; i++)
      {
        accumulator += phase;
        d_downchirp.push_back(gr_complex(std::conj(std::polar(1.0, accumulator))));
        d_upchirp.push_back(gr_complex(std::polar(1.0, accumulator)));
        phase += (2*M_PI)/d_fft_size;

        // f_mod.write((const char *)&d_upchirp[i], sizeof(gr_complex));
      }

    }

    /*
     * Our virtual destructor.
     */
    mod_impl::~mod_impl()
    {
    }

    void
    mod_impl::modulate (pmt::pmt_t msg)
    {
      pmt::pmt_t meta(pmt::car(msg));
      pmt::pmt_t symbols(pmt::cdr(msg));

      size_t pkt_len(0);
      size_t output_len = LORA_PREAMBLE_CHIRPS*d_fft_size   // Preamble
                          + 2*d_fft_size                    // Sync word
                          + 2*d_fft_size                    // SFD downchirps
                          + pkt_len*d_fft_size;             // Payload
      const uint16_t* symbols_in = pmt::u16vector_elements(symbols, pkt_len);

      std::vector<gr_complex> iq_out;

      std::cout << "MOD HERE 0" << std::endl;
      std::cout << "MOD pkt_len: " << pkt_len << std::endl;
      std::cout << "MOD len_chirp_table: " << d_upchirp.size() << std::endl;
      std::cout << "MOD d_fft_size: " << d_fft_size << std::endl;

      for (int i = 0; i < pkt_len; i++)
      {
        std::cout << "MOD INPUT: " << i << "\t\t" << symbols_in[i] << std::endl;
      } 

      // Preamble
      for (int i = 0; i < LORA_PREAMBLE_CHIRPS*d_fft_size; i++)
      {
        iq_out.push_back(d_upchirp[(i) % d_fft_size]);
      }

      std::cout << "MOD 0.0" << std::endl;

      // Sync Word 0
      for (int i = 0; i < d_fft_size; i++)
      {
        iq_out.push_back(d_upchirp[(LORA_SYNCWORD0 + i) % d_fft_size]);
      }

      std::cout << "MOD 0.1" << std::endl;

      // Sync Word 1
      for (int i = 0; i < d_fft_size; i++)
      {
        iq_out.push_back(d_upchirp[(LORA_SYNCWORD1 + i) % d_fft_size]);
      }

      std::cout << "MOD 0.2" << std::endl;

      // SFD Downchirps
      for (int i = 0; i < 2*d_fft_size; i++)
      {
        iq_out.push_back(d_downchirp[(LORA_SYNCWORD1 + i) % d_fft_size]);   // Downchirps start where sync word ends
      }

      std::cout << "MOD 0.3" << std::endl;

      // Payload
      for (int i = 0; i < pkt_len; i++)
      {
        for (int j = 0; j < d_fft_size; j++)
        {
          iq_out.push_back(d_upchirp[(symbols_in[i] + j) % d_fft_size]);
        }
      }

      std::cout << "MOD HERE 1" << std::endl;
      std::cout << "MOD pkt_len: " << pkt_len << std::endl;
      std::cout << "MOD iq_out.size(): " << iq_out.size() << std::endl;
      std::cout << "MOD num bytes required: " << iq_out.size()*sizeof(gr_complex) << std::endl;

      f_mod.write((const char *)&iq_out[0], iq_out.size()*sizeof(gr_complex));

      std::cout << "MOD HERE 2" << std::endl;

      pmt::pmt_t output = pmt::init_c32vector(iq_out.size(), iq_out);
      pmt::pmt_t msg_pair = pmt::cons(meta, output);
      message_port_pub(d_out_port, msg_pair);
    }

    void
    mod_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      /* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
    }

    int
    mod_impl::work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      // const unsigned short *in = (const unsigned short *) input_items[0];
      // gr_complex *out = (gr_complex *) output_items[0];

      // Do <+signal processing+>
      // Tell runtime system how many input items we consumed on
      // each input stream.
      // consume_each (noutput_items);

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace lora */
} /* namespace gr */

