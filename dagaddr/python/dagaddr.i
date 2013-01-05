%module dagaddr
%{
#include "../../include/dagaddr.hpp"
%}

%include "std_string.i"
%typemap (in) void*
{
    $1 = (void*)PyString_AsString($input);
}

/* Need these to avoid SWIG warnings (tells SWIG not to generate
   setters for these vars) */
%immutable XID_TYPE_UNKNOWN_STRING;
%immutable XID_TYPE_AD_STRING;
%immutable XID_TYPE_HID_STRING;
%immutable XID_TYPE_CID_STRING;
%immutable XID_TYPE_SID_STRING;
%immutable XID_TYPE_IP_STRING;

/* NOTE: It matters to swig where everything else in this file is
   with relation to this include, so don't move it. */
%include "../../include/dagaddr.hpp"

