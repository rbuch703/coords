
#ifndef VALIDATINGBIGINT_H
#define VALIDATINGBIGINT_H

#include <gmpxx.h>
#include "int128.h"


class ValidatingBigint
{
public:
    ValidatingBigint() {}
    ValidatingBigint( int32_t a): i(a), mpz(a) { assert(i.toMpz() == mpz); } 
    ValidatingBigint(uint32_t a): i(a), mpz(a) { assert(i.toMpz() == mpz); }
    ValidatingBigint( int64_t a): i(a), mpz(a) { assert(i.toMpz() == mpz); } 
    ValidatingBigint(uint64_t a): i(a), mpz(a) { assert(i.toMpz() == mpz); } 
    
    ValidatingBigint(int128_t pI, mpz_class pMpz): i(pI), mpz(pMpz)
    {
        if (i.toMpz() != mpz)
        std::cout << i.toMpz() << " vs. " << mpz << std::endl;
        assert(i.toMpz() == mpz);        
    }

    ValidatingBigint operator-() const { return ValidatingBigint( -i, -mpz); }

    double toDouble() const
    {
        assert(i.toMpz() == mpz);
        assert( i.toDouble() == mpz.get_d() );
        return i.toDouble();
        return 0.5;
    }
    
    explicit operator int64_t() const
    {
        assert ( mpz_class( (int64_t)i) == mpz);
        return (int64_t)i;
    }

    explicit operator uint64_t() const
    {
        assert ( mpz_class( (uint64_t)i) == mpz);
        return (uint64_t)i;
    }

    
private:
    int128_t  i;
    mpz_class mpz;
    
    friend ValidatingBigint operator+ (ValidatingBigint a, ValidatingBigint b);
    friend ValidatingBigint operator- (ValidatingBigint a, ValidatingBigint b);
    friend ValidatingBigint operator* (ValidatingBigint a, ValidatingBigint b);
    friend ValidatingBigint operator/ (ValidatingBigint a, uint32_t b);

    friend ValidatingBigint operator<<(ValidatingBigint a, uint32_t i);

    friend ValidatingBigint operator| (ValidatingBigint a, ValidatingBigint b);

    friend bool operator==(ValidatingBigint a, ValidatingBigint b);
    friend bool operator!=(ValidatingBigint a, ValidatingBigint b);
    friend bool operator< (const ValidatingBigint a, const ValidatingBigint b);
    friend bool operator<=(const ValidatingBigint a, const ValidatingBigint b);

    friend bool operator> (const ValidatingBigint a, const ValidatingBigint b);
    friend bool operator>=(const ValidatingBigint a, const ValidatingBigint b);

    friend ValidatingBigint abs(ValidatingBigint a);
    
    friend std::ostream& operator<<(std::ostream &os, ValidatingBigint a);

//    friend std::ostream& operator<<(std::ostream &os, int128_t a);
    
};


#endif
