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
#include "encode_impl.h"

#define HAMMING_P1_BITMASK 0x0D  // 0b00001101
#define HAMMING_P2_BITMASK 0x0B  // 0b00001011
#define HAMMING_P4_BITMASK 0x07  // 0b00000111
#define HAMMING_P8_BITMASK 0xFF  // 0b11111111

#define INTERLEAVER_BLOCK_SIZE 8

#define DEBUG_OUTPUT 0  // Controls debug print statements

namespace gr {
  namespace lora {

    encode::sptr
    encode::make( short spreading_factor,
                  short code_rate,
                  bool  low_data_rate,
                  bool  header)
    {
      return gnuradio::get_initial_sptr
        (new encode_impl(spreading_factor, code_rate, low_data_rate, header));
    }

    /*
     * The private constructor
     */
    encode_impl::encode_impl( short spreading_factor,
                              short code_rate,
                              bool  low_data_rate,
                              bool  header)
      : gr::block("encode",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0)),
        d_sf(spreading_factor),
        d_cr(code_rate),
        d_ldr(low_data_rate),
        d_header(header)
    {
      assert((d_sf > 5) && (d_sf < 13));
      assert((d_cr > 0) && (d_cr < 5));
      if (d_sf == 6) assert(!header);

      d_in_port = pmt::mp("in");
      d_out_port = pmt::mp("out");

      message_port_register_in(d_in_port);
      message_port_register_out(d_out_port);

      set_msg_handler(d_in_port, boost::bind(&encode_impl::encode, this, _1));

      switch(d_sf)
      {
        case 6:
          if (d_ldr) d_whitening_sequence = whitening_sequence_sf6_ldr_implicit;    // implicit header, LDR on
          else       d_whitening_sequence = whitening_sequence_sf6_implicit;        // implicit header, LDR on
          break;
        case 7:
          if (d_ldr) d_whitening_sequence = whitening_sequence_sf7_ldr_implicit;    // implicit header, LDR on
          else       d_whitening_sequence = whitening_sequence_sf7_implicit;        // implicit header, LDR on
          break;
        case 8:
          if (d_ldr) d_whitening_sequence = whitening_sequence_sf8_ldr_implicit;    // implicit header, LDR on
          else       d_whitening_sequence = whitening_sequence_sf8_implicit;        // implicit header, LDR on
          break;
        case 9:
          if (d_ldr) d_whitening_sequence = whitening_sequence_sf9_ldr_implicit;    // implicit header, LDR on
          else       d_whitening_sequence = whitening_sequence_sf9_implicit;        // implicit header, LDR on
          break;
        case 10:
          if (d_ldr) d_whitening_sequence = whitening_sequence_sf10_ldr_implicit;    // implicit header, LDR on
          else       d_whitening_sequence = whitening_sequence_sf10_implicit;        // implicit header, LDR on
          break;
        case 11:
          if (d_ldr) d_whitening_sequence = whitening_sequence_sf11_ldr_implicit;    // implicit header, LDR on
          else       d_whitening_sequence = whitening_sequence_sf11_implicit;        // implicit header, LDR on
          break;
        case 12:
          if (d_ldr) d_whitening_sequence = whitening_sequence_sf12_ldr_implicit;    // implicit header, LDR on
          else       d_whitening_sequence = whitening_sequence_sf12_implicit;        // implicit header, LDR on
          break;
        default:
          std::cerr << "Invalid spreading factor -- this state should never occur." << std::endl;
          d_whitening_sequence = whitening_sequence_sf8_implicit;   // TODO actually handle this
          break;
      }

      if (d_header)
      {
        std::cout << "Warning: Explicit header mode is not yet supported." << std::endl;
        std::cout << "         Using an implicit whitening sequence: modulation will work correctly; encoding will not." << std::endl;
      }

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
      for (int i = 0; i < symbols.size() && i < whitening_sequence_length; i++)
      {
        symbols[i] = ((unsigned char)(symbols[i] & 0xFF) ^ d_whitening_sequence[i]) & 0xFF;
      }
    }

    void
    encode_impl::print_bitwise_u8(std::vector<unsigned char> &buffer)
    {
      for (int i = 0; i < buffer.size(); i++)
      {
        std::cout << i << "\t" << std::bitset<8>(buffer[i] & 0xFF) << "\t";
        std::cout << std::hex << (buffer[i] & 0xFF) << std::endl;
      }
    }

    void
    encode_impl::print_bitwise_u16(std::vector<unsigned short> &buffer)
    {
      for (int i = 0; i < buffer.size(); i++)
      {
        std::cout << i << "\t" << std::bitset<16>(buffer[i] & 0xFFFF) << "\t";
        std::cout << std::hex << (buffer[i] & 0xFFFF) << std::endl;
      }
    }

    // Forward interleaver dimensions:
    //  PPM   == number of bits per symbol OUT of interleaver        AND number of codewords IN to interleaver
    //  RDD+4 == number of bits per codeword IN to interleaver       AND number of interleaved codewords OUT of interleaver
    //
    // bit width in:  (4+rdd)   block length: ppm
    // bit width out: ppm       block length: (4+rdd)
    void
    encode_impl::interleave(std::vector <unsigned char> &codewords,
                            std::vector <unsigned short> &symbols,
                            unsigned char ppm,
                            unsigned char rdd)
    {
      int bit_offset     = 0;
      int bit_idx        = 0;
      unsigned char block[INTERLEAVER_BLOCK_SIZE];    // maximum bit-width is 8, should RDD==4
      unsigned char reordered[INTERLEAVER_BLOCK_SIZE];

      // Block interleaver: interleave PPM codewords at a time into 4+RDD codewords
      for (int block_count = 0; block_count < codewords.size()/ppm; block_count++)
      {

        memset(block, 0, INTERLEAVER_BLOCK_SIZE*sizeof(unsigned char));
        bit_idx = 0;
        bit_offset = 0;

        if (ppm == 6)
        {
          reordered[4] = codewords[0 + block_count*ppm];
          reordered[5] = codewords[1 + block_count*ppm];
          reordered[2] = codewords[2 + block_count*ppm];
          reordered[3] = codewords[3 + block_count*ppm];
          reordered[0] = codewords[4 + block_count*ppm];
          reordered[1] = codewords[5 + block_count*ppm];
        }
        else if (ppm == 8)
        {
          reordered[0] = codewords[0 + block_count*ppm];
          reordered[7] = codewords[1 + block_count*ppm];
          reordered[2] = codewords[2 + block_count*ppm];
          reordered[1] = codewords[3 + block_count*ppm];
          reordered[4] = codewords[4 + block_count*ppm];
          reordered[3] = codewords[5 + block_count*ppm];
          reordered[6] = codewords[6 + block_count*ppm];
          reordered[5] = codewords[7 + block_count*ppm];
        }

        // Iterate through each bit in the interleaver block
        for (int bitcount = 0; bitcount < ppm*(4+rdd); bitcount++)
        {

          if (reordered[(bitcount / (4+rdd)) /*+ ppm*block_count*/] & 0x1 << (bitcount % (4+rdd)))
          {
            block[bitcount % (4+rdd)] |= ((0x1 << (ppm-1)) >> ((bit_idx + bit_offset) % ppm));   // integer divison in C++ is defined to floor
          }

          // bit idx walks through diagonal interleaving pattern
          if (bitcount % (4+rdd) == (4+rdd-1))
          {
            bit_idx = 0;
            bit_offset++;
          }
          else
          {
            bit_idx++;
          }
        }

        for (int block_idx = 0; block_idx < (4+rdd); block_idx++)
        {
          symbols.push_back(block[block_idx]);
        }
      }

      // Swap MSBs of each symbol within buffer (one of LoRa's quirks)
      for (int symbol_idx = 0; symbol_idx < symbols.size(); symbol_idx++)
      {
        symbols[symbol_idx] = ( (symbols[symbol_idx] &  (0x1 << (ppm-1))) >> 1 |
                                (symbols[symbol_idx] &  (0x1 << (ppm-2))) << 1 |
                                (symbols[symbol_idx] & ((0x1 << (ppm-2)) - 1))
                              );
      }
    }

    void
    encode_impl::hamming_encode(std::vector<unsigned char> &nybbles,
                                std::vector<unsigned char> &codewords,
                                unsigned char rdd)
    {
      unsigned char p1, p2, p4, p8;
      unsigned char mask;

      for (int i = 0; i < nybbles.size(); i++)
      {
        p1 = parity((unsigned char)nybbles[i], mask = (unsigned char)HAMMING_P1_BITMASK);
        p2 = parity((unsigned char)nybbles[i], mask = (unsigned char)HAMMING_P2_BITMASK);
        p4 = parity((unsigned char)nybbles[i], mask = (unsigned char)HAMMING_P4_BITMASK);
        p8 = parity((unsigned char)nybbles[i] | p1 << 7 | p2 << 6 | p4 << 4, 
                      mask = (unsigned char)HAMMING_P8_BITMASK);

        codewords.push_back(( (p1 << 7) |
                              (p2 << 6) |
                              (p8 << 5) |
                              (p4 << 4) |
                              (nybbles[i] & 0x08) | 
                              (nybbles[i] & 0x04) |
                              (nybbles[i] & 0x02) |
                              (nybbles[i] & 0x01)   ));

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
      pmt::pmt_t bytes(pmt::cdr(msg));

      size_t pkt_len(0);
      const uint8_t* bytes_in = pmt::u8vector_elements(bytes, pkt_len);

      std::vector<unsigned char>  nybbles;
      std::vector<unsigned char>  header_nybbles;
      std::vector<unsigned char>  payload_nybbles;
      std::vector<unsigned char>  header_codewords;
      std::vector<unsigned char>  payload_codewords;
      std::vector<unsigned short> header_symbols;
      std::vector<unsigned short> payload_symbols;
      std::vector<unsigned short> symbols;

      if (d_header)
      {
        nybbles.push_back(0xF);
        nybbles.push_back(0xF);
        nybbles.push_back(0xF);
        nybbles.push_back(0xF);
        nybbles.push_back(0xF);
        nybbles.push_back(0xF);
        nybbles.push_back(0xF);
        nybbles.push_back(0xF);
      }

      // split bytes into separate data nybbles
      for (int i = 0; i < pkt_len; i++)
      {
        nybbles.push_back((bytes_in[i] & 0xF0) >> 4);
        nybbles.push_back((bytes_in[i] & 0x0F));
      }

      // allocate nybbles to header or payload based on modulation parameters
      for (int i = 0; i < nybbles.size(); i++)
      {
        if (i < (d_sf-2)) // header
        {
          header_nybbles.push_back(nybbles[i]);
        }
        else // payload
        {
          payload_nybbles.push_back(nybbles[i]);
        }
      }

      #if DEBUG_OUTPUT
        std::cout << "Header Nybbles:" << std::endl;
        print_bitwise_u8(header_nybbles);
        std::cout << "Payload Nybbles:" << std::endl;
        print_bitwise_u8(payload_nybbles);
      #endif

      // Encode header
      hamming_encode(header_nybbles, header_codewords, 4);
      #if DEBUG_OUTPUT
        std::cout << "Header Codewords:" << std::endl;
        print_bitwise_u8(header_codewords);
      #endif

      interleave(header_codewords, header_symbols, d_sf-2, 4);
      #if DEBUG_OUTPUT
        std::cout << "Header Symbols:" << std::endl;
        print_bitwise_u16(header_symbols);
      #endif

      // Encode payload
      hamming_encode(payload_nybbles, payload_codewords, d_cr);
      #if DEBUG_OUTPUT
        std::cout << "Payload Codewords:" << std::endl;
        print_bitwise_u8(payload_codewords);
      #endif

      interleave(payload_codewords, payload_symbols, d_ldr ? (d_sf-2) : d_sf, d_cr);
      #if DEBUG_OUTPUT
        std::cout << "Payload Symbols:" << std::endl;
        print_bitwise_u16(payload_symbols);
      #endif

      // Combine symbol vectors
      symbols.insert(symbols.begin(), header_symbols.begin(), header_symbols.end());
      symbols.insert(symbols.end(), payload_symbols.begin(), payload_symbols.end());

      whiten(symbols);
      from_gray(symbols);

      // Expand symbol mapping for header or full packet if LDR enabled
      int ldr_limit = d_ldr ? symbols.size() : 8;
      for (int i = 0; i < symbols.size() && i < ldr_limit; i++)
      {
        symbols[i] <<= 2;
      }

      #if DEBUG_OUTPUT
        std::cout << "Modulated Symbols: " << std::endl;
        print_bitwise_u16(symbols);
      #endif

      pmt::pmt_t output = pmt::init_u16vector(symbols.size(), symbols);
      pmt::pmt_t msg_pair = pmt::cons(pmt::make_dict(), output);

      message_port_pub(d_out_port, msg_pair);
    }

  } /* namespace lora */
} /* namespace gr */
