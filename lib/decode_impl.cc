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
#include "decode_impl.h"

#define HAMMING_T1_BITMASK  0x52
#define HAMMING_T2_BITMASK  0x63
#define HAMMING_T4_BITMASK  0x78
#define HAMMING_T8_BITMASK  0x7F

namespace gr {
  namespace lora {

    decode::sptr
    decode::make()
    {
      return gnuradio::get_initial_sptr
        (new decode_impl());
    }

    /*
     * The private constructor
     */
    decode_impl::decode_impl()
      : gr::block("decode",
              gr::io_signature::make(1, 1, sizeof(unsigned short)),
              gr::io_signature::make(1, 1, sizeof(unsigned char)))
    {
      m_sf = 8;
      m_cr = 4; 

      m_whitening_sequence = (unsigned char *)malloc(100*sizeof(unsigned char));
    }

    /*
     * Our virtual destructor.
     */
    decode_impl::~decode_impl()
    {
      free(m_whitening_sequence);
    }

    void
    decode_impl::from_gray(std::vector<unsigned short> &symbols)
    {
      for (int i = 0; i < symbols.size(); i++)
      {
        symbols[i] = symbols[i] ^ (symbols[i] >> 16);
        symbols[i] = symbols[i] ^ (symbols[i] >>  8);
        symbols[i] = symbols[i] ^ (symbols[i] >>  4);
        symbols[i] = symbols[i] ^ (symbols[i] >>  2);
        symbols[i] = symbols[i] ^ (symbols[i] >>  1);
      }
    }

    void
    decode_impl::whiten(std::vector<unsigned short> &symbols)
    {
      for (int i = 0; i < symbols.size(); i++)
      {
        symbols[i] = ((unsigned char)(symbols[i] & 0xFF) ^ m_whitening_sequence[i]) & 0xFF;
      }
    }

    void
    decode_impl::deinterleave(std::vector <unsigned short> &symbols)
    {
    }

    void
    decode_impl::hamming_decode(std::vector<unsigned short> &codewords)
    {
      unsigned char t1, t2, t4, t8;
      unsigned char mask;
      char error_pos = 0;

      for (int i = 0; i < codewords.size(); i++)
      {
        t1 = parity((unsigned char)codewords[i], mask = (unsigned char)HAMMING_T1_BITMASK);
        t2 = parity((unsigned char)codewords[i], mask = (unsigned char)HAMMING_T2_BITMASK);
        t4 = parity((unsigned char)codewords[i], mask = (unsigned char)HAMMING_T4_BITMASK);
        t8 = parity((unsigned char)codewords[i], mask = (unsigned char)HAMMING_T8_BITMASK);

        error_pos = -1;
        if (t1 == 1) error_pos += 1;
        if (t2 == 1) error_pos += 2;
        if (t4 == 1) error_pos += 4;

        if (error_pos >= 0)
        {
          codewords[i] ^= 1 << error_pos;
        }

        codewords[i] = ((codewords[i] & 0x04) >> 2 | \
                        (codewords[i] & 0x10) >> 3 | \
                        (codewords[i] & 0x20) >> 4 | \
                        (codewords[i] & 0x30) >> 5) & 0x0F;
      }
    }

    unsigned char
    decode_impl::parity(unsigned char c, unsigned char bitmask)
    {
      unsigned char parity = 0;
      unsigned char shiftme = c & bitmask;

      for (int i = 0; i < 8; i++)
      {
        if (shiftme & 0x1) parity++;
        shiftme = shiftme >> 1;
      }

      return (parity % 2) ? 0 : 1;
    }

    void
    decode_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      /* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
    }

    int
    decode_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const unsigned short *in = (const unsigned short *) input_items[0];
      unsigned char *out = (unsigned char *) output_items[0];

      // Do <+signal processing+>
      // Tell runtime system how many input items we consumed on
      // each input stream.
      consume_each (noutput_items);

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace lora */
} /* namespace gr */

