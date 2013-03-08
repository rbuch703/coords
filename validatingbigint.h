
#ifndef VALIDATINGBIGINT_H
#define VALIDATINGBIGINT_H

#include <gmpxx.h>
#include "int128ng.h"

inline mpz_class toMpz(int128_t val) 
{
    mpz_class x = val.getHi();
    x = (x << 64) + val.getLo();
    return val.hasPositiveSign() ? x : mpz_class(-x);
}

class ValidatingBigint
{
public:
    ValidatingBigint() {}
    ValidatingBigint( int32_t a): i(a), mpz(a) { assert(toMpz(i) == mpz); } 
    ValidatingBigint(uint32_t a): i(a), mpz(a) { assert(toMpz(i) == mpz); }
    ValidatingBigint( int64_t a): i(a), mpz(a) { assert(toMpz(i) == mpz); } 
    ValidatingBigint(uint64_t a): i(a), mpz(a) { assert(toMpz(i) == mpz); } 
    ValidatingBigint(const char* c): i(0), mpz(0)
    { 
        bool neg = (c[0] == '-');
        if (neg) c++;
        while (*c)
        {
            assert ( *c >= '0' && *c <= '9');
            i = i*10 + (*c - '0');
            mpz=mpz*10 + (*c - '0');
            c++;
        }
        
        if (neg)
        {
            i = -i;
            mpz = -mpz;
        }
        assert( toMpz(i) == mpz);
        
        
    } 

    
    ValidatingBigint(int128_t pI, mpz_class pMpz): i(pI), mpz(pMpz)
    {
        if (toMpz(i) != mpz)
        std::cout << toMpz(i) << " vs. " << mpz << std::endl;
        assert(toMpz(i) == mpz);        
    }

    ValidatingBigint operator-() const { return ValidatingBigint( -i, -mpz); }

    double toDouble() const
    {
        //FIXME:  no validation here; the code for int128_t.toDouble() seems to work fine, but apparently rounds
        //        differently from the mpz implementation;
        return i.toDouble();
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

    ValidatingBigint operator*=(const ValidatingBigint &other)
    {
        i*= other.i;
        mpz*=other.mpz;
        
        assert( toMpz(i) == mpz);
        return *this;
    }
    
public:
    int128_t  i;
    mpz_class mpz;
private:    
    friend ValidatingBigint operator+ (ValidatingBigint a, ValidatingBigint b);
    friend ValidatingBigint operator- (ValidatingBigint a, ValidatingBigint b);
    friend ValidatingBigint operator* (ValidatingBigint a, ValidatingBigint b);
    friend ValidatingBigint operator/ (ValidatingBigint a, uint32_t b);
    friend ValidatingBigint operator% (ValidatingBigint a, uint32_t b);
    friend ValidatingBigint operator/ (ValidatingBigint a, ValidatingBigint b);
    friend ValidatingBigint operator% (ValidatingBigint a, ValidatingBigint b);

    friend ValidatingBigint operator<<(ValidatingBigint a, uint32_t i);
    friend ValidatingBigint operator>>(ValidatingBigint a, uint32_t i);

    friend ValidatingBigint operator| (ValidatingBigint a, ValidatingBigint b);

    friend bool operator==(ValidatingBigint a, ValidatingBigint b);
    friend bool operator!=(ValidatingBigint a, ValidatingBigint b);
    friend bool operator< (const ValidatingBigint a, const ValidatingBigint b);
    friend bool operator<=(const ValidatingBigint a, const ValidatingBigint b);

    friend bool operator> (const ValidatingBigint a, const ValidatingBigint b);
    friend bool operator>=(const ValidatingBigint a, const ValidatingBigint b);

    friend ValidatingBigint abs(ValidatingBigint a);
    //friend ValidatingBigint operator~(ValidatingBigint a);

    friend std::ostream& operator<<(std::ostream &os, ValidatingBigint a);

//    friend std::ostream& operator<<(std::ostream &os, int128_t a);
    
};


#endif
