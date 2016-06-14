/* -*- c++ -*- */

#define LORA_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "lora_swig_doc.i"

%{
#include "lora/demod.h"
#include "lora/decode.h"
#include "lora/mod.h"
#include "lora/encode.h"
%}


%include "lora/demod.h"
GR_SWIG_BLOCK_MAGIC2(lora, demod);
%include "lora/decode.h"
GR_SWIG_BLOCK_MAGIC2(lora, decode);
%include "lora/mod.h"
GR_SWIG_BLOCK_MAGIC2(lora, mod);
%include "lora/encode.h"
GR_SWIG_BLOCK_MAGIC2(lora, encode);
