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
#include "encode_impl.h"
#include "stdio.h"

#define HAMMING_T1_BITMASK 0x0D  // 0b00001101
#define HAMMING_T2_BITMASK 0x0B  // 0b00001011
#define HAMMING_T4_BITMASK 0x07  // 0b00000111
#define HAMMING_T8_BITMASK 0xFF  // 0b11111111

#define INTERLEAVER_BLOCK_SIZE 8

namespace gr {
  namespace lora {

    encode::sptr
    encode::make( short spreading_factor,
                  short code_rate)
    {
      return gnuradio::get_initial_sptr
        (new encode_impl(spreading_factor, code_rate));
    }

    /*
     * The private constructor
     */
    encode_impl::encode_impl( short spreading_factor,
                              short code_rate)
      : gr::block("encode",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0))
              // gr::io_signature::make(1, 1, sizeof(unsigned char)),
              // gr::io_signature::make(1, 1, sizeof(unsigned short)))
    {
      d_in_port = pmt::mp("in");
      d_out_port = pmt::mp("out");

      message_port_register_in(d_in_port);
      message_port_register_out(d_out_port);

      set_msg_handler(d_in_port, boost::bind(&encode_impl::encode, this, _1));

      d_sf = spreading_factor;
      d_cr = code_rate;

      d_interleaver_size = d_sf;

      d_fft_size = (1 << spreading_factor);
    }

    /*
     * Our virtual destructor.
     */
    encode_impl::~encode_impl()
    {
    }

    void
    encode_impl::to_gray(std::vector<unsigned short> &symbols)
    {
      for (int i = 0; i < symbols.size(); i++)
      {
        symbols[i] = (symbols[i] >> 1) ^ symbols[i];
      }
    }

    void
    encode_impl::from_gray(std::vector<unsigned short> &symbols)
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
    encode_impl::whiten(std::vector<unsigned short> &symbols)
    {
      for (int i = 0; i < symbols.size(); i++)
      {
        symbols[i] = ((unsigned char)(symbols[i] & 0xFF) ^ d_whitening_sequence[i]) & 0xFF;
      }
    }

    void
    encode_impl::il_block_print(unsigned char *block)
    {
      for (int i = 0; i < INTERLEAVER_BLOCK_SIZE; i++)
      {
        for (int j = INTERLEAVER_BLOCK_SIZE-1; j >=0 ; j--)
        {
          printf("%d ", (block[i] & (0x01 << j)) ? 1 : 0);
        }
        printf("\n");
      }
    }

    void
    encode_impl::interleave(std::vector <unsigned short> &codewords,
                            std::vector <unsigned short> &symbols)
    {
      unsigned char ppm = 8;
      int inner = 0;
      int outer = 0;
      unsigned char block[INTERLEAVER_BLOCK_SIZE] = {0};
      unsigned char word = 0;
      unsigned char temp_word = 0;

      // for (outer = 0; outer < codewords.size()/ppm; outer++)
      // {
      //   memset(block, 0, d_interleaver_size*sizeof(unsigned char));

      //   for (inner = 0; inner < INTERLEAVER_BLOCK_SIZE; inner++)
      //   {
      //     block[inner] = (codeword[outer+inner*INTERLEAVER_BLOCK_SIZE] << inner) | (codeword[outer+inner*INTERLEAVER_BLOCK_SIZE] >> INTERLEAVER_BLOCK_SIZE-inner);
      //   }

      //   il_block_print(block);

      //   // for (inner = 0; inner < ppm; inner++)
      //   // {
      //   //   // Rotate codeword within frame
      //   //   word = (word >> (inner-1)%ppm) | (word << (ppm-inner+1)%ppm);
      //   //   // word = (word >> (inner+1)%ppm) | (word << (ppm-inner-1)%ppm);

      //   //   word = ((word & 128) >> 7) | ((word & 64) >> 5) | ((word & 32) >> 3) | ((word & 16) >> 1) | ((word & 8) << 1) | ((word & 4) << 3) | ((word & 2) << 5) | ((word & 1) << 7);

      //   //   word = ((128 & symbols[ppm*outer+(inner+2)%ppm]) >> 1) | ((64 & symbols[ppm*outer+(inner+1)%ppm]) << 1) | (32 & symbols[ppm*outer+(inner+3)%ppm]) | (16 & symbols[ppm*outer+(inner+4)%ppm]) \
      //   //   | (8 & symbols[ppm*outer+(inner+5)%ppm]) | (4 & symbols[ppm*outer+(inner+6)%ppm]) | (2 & symbols[ppm*outer+(inner+7)%ppm]) | (1 & symbols[ppm*outer+(inner+8)%ppm]);

      //   // }

      //   for (int i = 0; i < INTERLEAVER_BLOCK_SIZE; i++) {
      //     symbols.push_back(block[i]);
      //   }
      // }

      // // for (outer = 0; outer < codewords.size()/ppm; outer++)
      // // {
      // //   memset(block, 0, INTERLEAVER_BLOCK_SIZE*sizeof(unsigned char));

      // //   // codewords[i+0] = codewords[i+0];
      // //   // codewords[i+1] = codewords[i+7];
      // //   // codewords[i+2] = codewords[i+2];
      // //   // codewords[i+3] = codewords[i+1];
      // //   // codewords[i+4] = codewords[i+4];
      // //   // codewords[i+5] = codewords[i+3];
      // //   // codewords[i+6] = codewords[i+6];
      // //   // codewords[i+7] = codewords[i+5];

      // //   for (inner = 0; inner < ppm; inner++)
      // //   {
      // //     // Most significant bits are flipped
      // //     word = ((128 & symbols[ppm*outer+(inner+2)%ppm]) >> 1) | ((64 & symbols[ppm*outer+(inner+1)%ppm]) << 1) | (32 & symbols[ppm*outer+(inner+3)%ppm]) | (16 & symbols[ppm*outer+(inner+4)%ppm]) \
      // //               | (8 & symbols[ppm*outer+(inner+5)%ppm]) | (4 & symbols[ppm*outer+(inner+6)%ppm]) | (2 & symbols[ppm*outer+(inner+7)%ppm]) | (1 & symbols[ppm*outer+(inner+8)%ppm]);

      // //     // Reverse endianness
      // //     word = ((word & 128) >> 7) | ((word & 64) >> 5) | ((word & 32) >> 3) | ((word & 16) >> 1) | ((word & 8) << 1) | ((word & 4) << 3) | ((word & 2) << 5) | ((word & 1) << 7);
      // //     word &= 0xFF;

      // //     // Rotate
      // //     word = (word << (inner+1)%ppm) | (word >> (ppm-inner-1)%ppm);
      // //     word &= 0xFF;

      // //     // Reorder into hamming format. Hamming parity bits p3 and p4 are flipped OTA, so format to be corrected is p1,p2,p4,p3,d1,d2,d3,d4
      // //     // Final bit order is p1,p2,d1,p3,d2,d3,d4,p4
      // //     word = ((word & 128)) | ((word & 64)) | ((word & 32) >> 5) | ((word & 16)) | ((word & 8) << 2) | ((word & 4) << 1) | ((word & 2) << 1) | ((word & 1) << 1);

      // //     block[inner] = word & 0xFF;
      // //   }

      // //   symbols.push_back(block[0]);
      // //   symbols.push_back(block[7]);
      // //   symbols.push_back(block[2]);
      // //   symbols.push_back(block[1]);
      // //   sym bols.push_back(block[4]);
      // //   symbols.push_back(block[3]);
      // //   symbols.push_back(block[6]);
      // //   symbols.push_back(block[5]);
      // // }

      for (int i = 0; i < codewords.size(); i++)
      {
        symbols.push_back(codewords[i]);
      }
    }

    void
    encode_impl::hamming_encode(std::vector<unsigned char> &nybbles,
                                std::vector<unsigned short> &codewords)
    {
      unsigned char t1, t2, t4, t8;
      unsigned char mask;

      for (int i = 0; i < nybbles.size(); i++)
      {
        t1 = parity((unsigned char)nybbles[i], mask = (unsigned char)HAMMING_T1_BITMASK);
        t2 = parity((unsigned char)nybbles[i], mask = (unsigned char)HAMMING_T2_BITMASK);
        t4 = parity((unsigned char)nybbles[i], mask = (unsigned char)HAMMING_T4_BITMASK);
        t8 = parity((unsigned char)nybbles[i] | t1 << 7 | t2 << 6 | t4 << 4, 
                      mask = (unsigned char)HAMMING_T8_BITMASK);

        codewords.push_back(( (t1 << 7) |
                              (t2 << 6) |
                              (t8 << 5) |
                              (t4 << 4) |
                              (nybbles[i] & 0x08) | 
                              (nybbles[i] & 0x04) |
                              (nybbles[i] & 0x02) |
                              (nybbles[i] & 0x01)   ));

        printf("DATA:     %d\t\t", nybbles[i]);
        printf("CODEWORD: %d\n", codewords[i]);
      }
    }

    unsigned char
    encode_impl::parity(unsigned char c, unsigned char bitmask)
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
    encode_impl::print_payload(std::vector<unsigned char> &payload)
    {
        std::cout << "Encoded LoRa packet (hex): ";
        for (int i = 0; i < payload.size(); i++)
        {
          std::cout << std::hex << (unsigned int)payload[i] << " ";
        }
        std::cout << std::endl;
    }

    void
    encode_impl::encode (pmt::pmt_t msg)
    {
      // pmt::pmt_t meta(pmt::car(msg));
      pmt::pmt_t bytes(pmt::cdr(msg));

      size_t pkt_len(0);
      const uint8_t* bytes_in = pmt::u8vector_elements(bytes, pkt_len);

      std::cout << "ENCODE pkt_len: " << pkt_len << std::endl;

      std::vector<unsigned char> nybbles;
      std::vector<unsigned short> symbols;
      std::vector<unsigned short> codewords;

      for (int i = 0; i < pkt_len; i++)
      {
        nybbles.push_back((bytes_in[i] & 0xF0) >> 4);
        nybbles.push_back((bytes_in[i] & 0x0F));
      }

      hamming_encode(nybbles, codewords);
      interleave(codewords, symbols);
      whiten(symbols);
      from_gray(symbols);

      pmt::pmt_t output = pmt::init_u16vector(symbols.size(), symbols);
      pmt::pmt_t msg_pair = pmt::cons(pmt::make_dict(), output);

      std::cout << "ENCODE symbols_size" << symbols.size() << std::endl;
      std::cout << "ENCODE msg_pair: " << output << std::endl;

      message_port_pub(d_out_port, msg_pair);
    }

    void
    encode_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      // <+forecast+> e.g. ninput_items_required[0] = noutput_items 
      ninput_items_required[0] = noutput_items;
    }

    int
    encode_impl::work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      return noutput_items;
    }    

  } /* namespace lora */
} /* namespace gr */

#if 0
    int
    encode_impl::work (int noutput_items,
    // encode_impl::encode (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const unsigned char *in = (const unsigned char *) input_items[0];
      unsigned short *out = (unsigned short *) output_items[0];
      unsigned int data_length = ninput_items[0];

      std::vector<unsigned char> nybbles;

      std::cout << "HERE" << std::endl;

      d_codewords.clear();

      // Split 8-bit bytes into nybbles for FEC processing
      for(int i=0; i < data_length; i++)
      {
        nybbles.push_back((in[i] & 0xF0) >> 4);
        nybbles.push_back((in[i] & 0x0F));
      }

      hamming_encode(nybbles, d_codewords);

      // Do <+signal processing+>
      // Tell runtime system how many input items we consumed on
      // each input stream.
      consume_each (noutput_items);

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }
#endif