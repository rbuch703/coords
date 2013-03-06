
#ifndef CONFIG_H
#define CONFIG_H


#if 0
#include <gmpxx.h>
typedef mpz_class BigInt;
#endif


#if false
    #include "validatingbigint.h" 
    typedef ValidatingBigint BigInt;
#else
    #include "int128.h" 
    typedef int128_t BigInt;
#endif


//typedef int64_t BigInt;

#include "fraction.h"
typedef Fraction<BigInt> BigFraction;

#endif

