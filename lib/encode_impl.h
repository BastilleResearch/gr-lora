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

#ifndef INCLUDED_LORA_ENCODE_IMPL_H
#define INCLUDED_LORA_ENCODE_IMPL_H

#include <volk/volk.h>
#include <bitset>
#include <lora/encode.h>

#define WHITENING_SEQUENCE_LENGTH 219

namespace gr {
  namespace lora {

    class encode_impl : public encode
    {
     private:
      const unsigned char d_whitening_sequence[WHITENING_SEQUENCE_LENGTH] = {67, 35, 147, 217, 235, 141, 213, 91, 52, 201, 118, 77, 119, 218, 84, 78, 194, 103, 44, 222, 208, 9, 77, 226, 47, 180, 222, 183, 75, 98, 17, 185, 220, 91, 148, 11, 104, 242, 13, 21, 50, 133, 104, 81, 243, 134, 54, 93, 172, 232, 49, 227, 164, 166, 164, 202, 162, 164, 168, 144, 194, 45, 142, 39, 142, 60, 152, 176, 7, 102, 56, 252, 20, 8, 240, 6, 14, 108, 222, 26, 104, 240, 65, 195, 70, 125, 115, 196, 190, 215, 138, 55, 124, 201, 174, 184, 183, 142, 60, 120, 163, 190, 180, 172, 171, 181, 24, 240, 136, 1, 250, 61, 247, 132, 185, 49, 78, 119, 147, 73, 204, 43, 54, 204, 85, 85, 36, 243, 75, 50, 199, 140, 63, 203, 232, 156, 16, 65, 230, 45, 238, 188, 146, 142, 32, 227, 100, 234, 148, 19, 1, 181, 131, 108, 168, 179, 72, 75, 68, 140, 106, 180, 217, 176, 32, 246, 88, 43, 151, 76, 186, 18, 185, 0, 27, 113, 45, 191, 93, 219, 184, 144, 91, 67, 210, 68, 187, 19, 240, 16, 82, 227, 37, 168, 113, 196, 112, 194, 68, 37, 201, 115, 140, 155, 138, 34, 62, 255, 85, 15, 251, 52, 12, 81, 190, 13, 68, 126, 125};

      pmt::pmt_t d_in_port;
      pmt::pmt_t d_out_port;
      
      unsigned char d_sf;
      unsigned char d_cr;
      unsigned char d_interleaver_size;
      bool d_header;

      unsigned short d_fft_size;

     public:
      encode_impl(  short spreading_factor,
                    short code_rate,
                    bool  header);
      ~encode_impl();

      void to_gray(std::vector<unsigned short> &symbols);
      void from_gray(std::vector<unsigned short> &symbols);
      void whiten(std::vector<unsigned short> &symbols);
      void interleave(std::vector<unsigned short> &symbols, std::vector<unsigned short> &codewords);
      void hamming_encode(std::vector<unsigned char> &nybbles, std::vector<unsigned short> &codewords);
      unsigned char parity(unsigned char c, unsigned char bitmask);
      void print_payload(std::vector<unsigned char> &payload);

      void print_bitwise_u8 (std::vector<unsigned char>  &buffer);
      void print_bitwise_u16(std::vector<unsigned short> &buffer);

      void encode(pmt::pmt_t msg);

    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_ENCODE_IMPL_H */

