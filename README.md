# gr-lora

This is an open-source implementation of the LoRa CSS PHY, based on the blind signal analysis conducted by @matt-knight.  The original research that guided this implementation may be found at https://github.com/matt-knight/research

## LoRa
LoRa is a wireless LPWAN PHY that is developed and maintained by Semtech.  It is designed to provide long range, low data rate connectivity to IoT-focused devices.  A reasonable analogy is that LoRa is like cellular data service, but optimized for embedded devices.

LoRa uses a unique CSS modulation that modulates data onto chirps.  For resiliency it uses a multi-stage encoding pipeline that includes Hamming FEC, interleaving, data whitening, and gray encoding of symbols.

Since LoRa is closed-source, developers and security researchers have been limited to interacting with it via Layer 2+ protocols and APIs exposed through integrated circuits.  However, the security of the PHY layer cannot be taken for granted: numerous powerful 802.15.4 attacks exploit its PHY layer to inject and mask malicious traffic in ways that are invisible to higher layers.  Our hope is that this module will empower developers and researchers alike to explore this nascent protocol and make it more secure.

## Design
Modulation and encoding stages are modeled as separate blocks to allow for modularity.  The asynchronous PDU interface is used to pass messages to/from the encoder/decoder and between encoding and modulating stages.  A good way to interface with the blocks is to use a Socket PDU block configured as a UDP Server, which can be written to like any other socket via ```nc -u [IP] [PORT]```.

The modulator and demodulator blocks do not resample or channelize input/output IQ streams; they expect to be provided a stream that is channelized to the bandwidth of the modulation.

## Configuration
- Spreading Factor: Number of bits per symbol, typically [6:12].
- Code Rate / # Parity Bits: Order of the Hamming FEC used.  The number of data bits per codeword is always 4, but the number of parity bits can range from [1:4].
- Header: Whether frames produced/consumed by the frame will contain the 8-symbol header.
- FFT Window Beta: Controls the shape of the Kaiser windowing curve that is applied to the FFT input IQ.
- FFT Size Factor: Multiplier applied to the width/number of bins of the FFT.  A multiplier of 1 yields 2\*\*spreading_factor bins, the minimum number required by the modulation.  Received symbols are divided down to map within the valid range of [0:(2\*\*sf)-1].  Initial experimentation reveals 2 yields good performance.

## Installation
```
git clone git://github.com/BastilleResearch/gr-lora.git
cd gr-lora
mkdir build
cd build
cmake ../
make
sudo make install
sudo ldconfig
```

## Usage
Example flowgraphs are provided in the examples/ directory.  Socket PDUs are used as the primary input interface.  Socket PDUs configured as UDP Servers may be connected to via:
```$ nc -u localhost 52001```

## Limitations and TODOs
- Implicit PHY header mode: Implicit PHY header mode is not yet supported.
- PHY CRC support: Ditto for the PHY CRC(s).
- Additional demodulation strategies: iterating on existing strategy (oversampling, using oversized FFTs, etc.) and trying other methods for detection and sync.
- Implement upper layers (LoRaWAN): For further integration and experimentation.

## Acknowledgements
- Balint Seeber and the Bastille Threat Research Team
- IQ Donors Thomas Telkamp and Chuck Swiger

