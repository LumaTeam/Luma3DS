//---------------------------------------------------------------------------------
#ifndef _font_h_
#define _font_h_
//---------------------------------------------------------------------------------
#if defined FONT_6X10
#include "font_6x10.h"
#elif defined FONT_ACORN
#include "font_acorn_8x8.h"
#elif defined FONT_GB
#include "font_gb_7x6.h"
#else
#include "font_orig.h" // if nothing is selected
#endif 
//---------------------------------------------------------------------------------
#endif //_font_h_
//---------------------------------------------------------------------------------
