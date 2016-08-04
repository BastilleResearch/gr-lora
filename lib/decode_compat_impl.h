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

#ifndef INCLUDED_LORA_DECODE_COMPAT_IMPL_H
#define INCLUDED_LORA_DECODE_COMPAT_IMPL_H

#include <lora/decode_compat.h>

namespace gr {
  namespace lora {

    class decode_compat_impl : public decode_compat
    {
     private:
      unsigned short m_sf;
      unsigned short m_cr;
      unsigned short m_fft_size;

      std::vector<unsigned short> m_symbols;
      std::vector<unsigned char>  m_symbols_char;
      std::vector<unsigned char>  m_codewords;
      std::vector<unsigned char>  m_bytes;
      // Nothing to declare in this block.

     public:
      decode_compat_impl(  short spreading_factor,
                           short code_rate);
      ~decode_compat_impl();

      void to_gray(std::vector<unsigned short> &symbols);
      void from_gray(std::vector<unsigned short> &symbols);
      void whiten(std::vector<unsigned char> &symbols);
      void deinterleave(std::vector<unsigned char> &symbols, std::vector<unsigned char> &codewords);
      void hamming_decode(std::vector<unsigned char> &codewords, std::vector<unsigned char> &bytes);
      unsigned char parity(unsigned char c, unsigned char bitmask);
      void print_payload(std::vector<unsigned char> &payload);

      void forecast(int noutput_items, gr_vector_int &ninput_items_required);

      // Where all the action really happens
      int general_work(int noutput_items,
         gr_vector_int &ninput_items,
         gr_vector_const_void_star &input_items,
         gr_vector_void_star &output_items);
    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_DECODE_COMPAT_IMPL_H */

