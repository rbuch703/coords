
#ifndef CONFIG_H
#define CONFIG_H


#if 0
#include <gmpxx.h>
typedef mpz_class BigInt;
#endif

#if true
#include "validatingbigint.h"
typedef ValidatingBigint BigInt;
#else
#include "int128.h"
typedef int128_t BigInt;
#endif

#endif

