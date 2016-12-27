#!/usr/bin/env python2
# LoRa Whitening Sequence Consensus Generator
# @mknight
#
# Ingests CSV file containing binary strings, produces the best match among all of them
# Assumes INPUT_BIT_WIDTH wide string, as printed from C++ std::bitset<INPUT_BIT_WIDTH>

import argparse

INPUT_BIT_WIDTH = 16

def main():
  parser = argparse.ArgumentParser(description='LoRa Whitening Sequence Consensus Utility')
  parser.add_argument('--filename', type=str, help='input file containing binary CSV')
  parser.add_argument('--ppm', type=int, help='PPM/spreading factor of input/desired consensus')
  parser.add_argument('--header_mode', type=str, default='implicit', help='header_mode (explicit or implicit(default))')
  parser.add_argument('--ldr', type=str, default='', help='low_data_rate (ldr or off(default))')

  args = parser.parse_args()

  ifile = open(args.filename, 'r')

  seqs = []
  for line in ifile:
    line = line.strip(',')
    line = line.strip('\n')
    line = line.split(' ')
    seqs += [line[1:]]

  # Only compare up to the shortest string in the list
  shortest_len = len(min(seqs, key=len))

  consensus = []

  # Iterate through all whitening symbols
  for i in range(shortest_len):
    consensus += [0]

    # Iterate through PPM bit positions within the current whitening symbol
    for bit in range(args.ppm):
      bitlist = []

      # Check the value of this whitening symbol/bit position among all whitening strings
      for s in seqs:
        if s[i][INPUT_BIT_WIDTH-bit-1] is '0':
          bitlist += [0]
        else:
          bitlist += [1]

      # Set the bit in the consensus list if appropriate
      if bitlist.count(1) > bitlist.count(0):
        consensus[i] |= 0x1 << bit

    # print bin(consensus[0])
    # return

  output_filename = 'sf' + str(args.ppm) + '_whitening_declaration_' + args.header_mode + '.txt'
  ofile = open(output_filename, 'w')

  ofile.write('const unsigned short whitening_sequence_sf' + str(args.ppm) + '_' + str(args.ldr) + '_' + args.header_mode + '[' + str(shortest_len) + '] = {')

  for i in range(shortest_len):
    if i != 0:
      ofile.write(', ')
    ofile.write('0x')
    ofile.write('{:04x}'.format(consensus[i]))
    # print '{:04x}'.format(consensus[i]),

  # print '\n'
  ofile.write('};\n')
  ofile.close()

  ifile.close()

  return


if __name__ == '__main__':
  main()