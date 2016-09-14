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
#include "decode_compat_impl.h"
#include "lora/decode.h"

namespace gr {
  namespace lora {

    decode_compat::sptr
    decode_compat::make(  short spreading_factor,
                          short code_rate)
    {
      return gnuradio::get_initial_sptr
        (new decode_compat_impl(8, 4));
    }

    /*
     * The private constructor
     */
    decode_compat_impl::decode_compat_impl(  short spreading_factor,
                                             short code_rate)
      : gr::block("decode_compat",
              gr::io_signature::make(1, 1, sizeof(short)),
              gr::io_signature::make(0, 1, sizeof(unsigned char)))
    {
      m_sf = 8;
      m_cr = 4; 

      m_fft_size = (1 << spreading_factor);
    }

    /*
     * Our virtual destructor.
     */
    decode_compat_impl::~decode_compat_impl()
    {
    }

    void
    decode_compat_impl::to_gray(std::vector<unsigned short> &symbols)
    {
      for (int i = 0; i < symbols.size(); i++)
      {
        symbols[i] = (symbols[i] >> 1) ^ symbols[i];
      }
    }

    void
    decode_compat_impl::from_gray(std::vector<unsigned short> &symbols)
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
    decode_compat_impl::whiten(std::vector<unsigned char> &symbols)
    {
      unsigned char WhiteningKeyMSB; // Global variable so the value is kept after starting the
      unsigned char WhiteningKeyLSB; // de-whitening process
      WhiteningKeyMSB = 0x01; // Init value for the LFSR, these values should be initialize only
      WhiteningKeyLSB = 0xFF; // at the start of a whitening or a de-whitening process
      // *buffer is a char pointer indicating the data to be whiten / de-whiten
      // buffersize is the number of char to be whiten / de-whiten
      // >> The whitened / de-whitened data are directly placed into the pointer
      unsigned char  i = 0;
      unsigned short j = 0;
      unsigned char  WhiteningKeyMSBPrevious = 0; // 9th bit of the LFSR
      for( j = 0; j < symbols.size(); j++ )     // byte counter
      {
          symbols[j] ^= WhiteningKeyLSB;   // XOR between the data and the whitening key
          for( i = 0; i < 8; i++ )    // 8-bit shift between each byte
          {
              WhiteningKeyMSBPrevious = WhiteningKeyMSB;
              WhiteningKeyMSB = ( WhiteningKeyLSB & 0x01 ) ^ ( ( WhiteningKeyLSB >> 5 ) & 0x01 );
              WhiteningKeyLSB= ( ( WhiteningKeyLSB >> 1 ) & 0xFF ) | ( ( WhiteningKeyMSBPrevious << 7 ) & 0x80 );
          }
      }
    }

    void
    decode_compat_impl::deinterleave(std::vector<unsigned char> &symbols, std::vector<unsigned char> &codewords)
    {
      unsigned char RDD = 4;
      unsigned char PPM = 8;

      // unsigned char block[8] = {0};

      for (unsigned short x = 0; x < symbols.size()/(4 + RDD); x++)
      {
        // memset(block, 0, PPM*sizeof(unsigned char));
        const unsigned short cwOff = x*PPM;
        const unsigned short symOff = x*(4 + RDD);
        for (unsigned short k = 0; k < 4 + RDD; k++)
        {
          for (unsigned short m = 0; m < PPM; m++)
          {
            const unsigned short i = (m-k+PPM)%PPM;
            const unsigned char bit = (symbols[symOff + k] >> m) & 0x1;
            codewords[cwOff + i] |= (bit << k);
          }
        }
        // codewords.push_back(block[0]);
        // codewords.push_back(block[1]);
        // codewords.push_back(block[2]);
        // codewords.push_back(block[3]);
        // codewords.push_back(block[4]);
        // codewords.push_back(block[5]);
        // codewords.push_back(block[6]);
        // codewords.push_back(block[7]);
      }
    }

    void
    decode_compat_impl::hamming_decode(std::vector<unsigned char> &codewords,
                                std::vector<unsigned char> &bytes)
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
        if (t1 != 0) error_pos += 1;
        if (t2 != 0) error_pos += 2;
        if (t4 != 0) error_pos += 4;

        if (error_pos >= 0)
        {
          codewords[i] ^= 0x80 >> error_pos;
        }

        codewords[i] = ((codewords[i] & 0x20) >> 2 | \
                        (codewords[i] & 0x08) >> 1 | \
                        (codewords[i] & 0x04) >> 1 | \
                        (codewords[i] & 0x02) >> 1) & 0x0F;

        if (i%2 == 1)
        {
          bytes.push_back(((codewords[i-1] & 0x0F) << 4) | (codewords[i] & 0x0F));
        }
      }
    }

    unsigned char
    decode_compat_impl::parity(unsigned char c, unsigned char bitmask)
    {
      unsigned char parity = 0;
      unsigned char shiftme = c & bitmask;

      for (int i = 0; i < 8; i++)
      {
        if (shiftme & 0x1) parity++;
        shiftme = shiftme >> 1;
      }

      return parity % 2;
    }

    void
    decode_compat_impl::print_payload(std::vector<unsigned char> &payload)
    {
        std::cout << "Received LoRa packet (hex): ";
        for (int i = 0; i < payload.size(); i++)
        {
          std::cout << std::hex << (unsigned int)payload[i] << " ";
        }
        std::cout << std::endl;
    }

    void
    decode_compat_impl::forecast(int noutput_items, gr_vector_int &ninput_items_required)
    {
       // <+forecast+> e.g. ninput_items_required[0] = noutput_items
       ninput_items_required[0] = noutput_items; 
    }

    int
    decode_compat_impl::general_work(int noutput_items,
        gr_vector_int &ninput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
    {
      const unsigned short *in = (const unsigned short *) input_items[0];

      // Do <+signal processing+>
      unsigned short symbol = (unsigned short)*in;

      m_symbols.push_back(symbol);

      if (symbol == m_fft_size)        // MAGIC word
      {
        to_gray(m_symbols);

        for (int i = 0; i < m_symbols.size(); i++)
        {
          m_symbols_char.push_back((unsigned char)m_symbols[i]);
        }

        whiten(m_symbols_char);

        // Remove header until whitening sequence is extended
        m_symbols.erase(m_symbols.begin(), m_symbols.begin()+8);

        unsigned short numCodewords = (m_symbols.size()/(4 + 4))*8;
        std::vector<uint8_t> codewords(numCodewords);

        deinterleave(m_symbols_char, codewords);

        hamming_decode(codewords, m_bytes);

        print_payload(m_bytes);

        m_symbols.clear();
        m_symbols_char.clear();
        m_codewords.clear();
        m_bytes.clear();
      }
      else if (m_symbols.size() > SYMBOL_TIMEOUT_COUNT)
      {
        m_symbols.clear();
      }

      consume_each (noutput_items);

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }


  } /* namespace lora */
} /* namespace gr */

