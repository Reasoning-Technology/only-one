#ifndef PARSE_STATUS_H
#define PARSE_STATUS_H

#include "types.h"

  // comp bug enum class didn't work .. soo I can make bit flags instead
  struct ParseStatus{
    ParseStatus(){ value = UnInitialized; }
    ParseStatus(uchar value):value(value){;}
    uchar value;
    bool operator == (const ParseStatus other) const{ RETURN value==other.value; }
    bool operator != (const ParseStatus other) const{ RETURN value!=other.value; }

    const static uchar UnInitialized    =0b00000000;
    const static uchar Found            =0b00000001;
    const static uchar NotFound         =0b00000010;
    const static uchar NoObject         =0b00000100;
    const static uchar NullObject       =0b00001000;
    const static uchar Malformed        =0b00010000;
    const static uchar MaxMisparseCount =0b00100000;
    const static uchar MaxErrors        =0b01000000;
    
  };

#endif
