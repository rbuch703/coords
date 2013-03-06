
#ifndef INT128_H
#define INT128_H

#include <stdint.h>
#include <assert.h>

//#include <gmpxx.h>

#include <iostream>

class int128_t
{
public:
    int128_t() {hi = lo = 0; isPositive = true;}
    int128_t(uint32_t a) { hi = 0; lo = a; isPositive = true;}
    int128_t(int32_t a) 
    { 
        hi = 0;
        isPositive = ! (a & 0x80000000); // sign bit is set
        lo = isPositive ? a : ((~a) +1); //two's complement; +1 cannot overflow since source is only 32 bit
    }

    int128_t(int64_t a): hi(0)
    { 
        isPositive = ! (a & 0x8000000000000000ull);
        lo = isPositive ? a : ((~a) + 1);
    }

    int128_t(uint64_t a): isPositive(true), hi(0)
    { 
        lo = a;
    }

 
    int128_t operator-() const { return int128_t(!isPositive, hi, lo); }

    double toDouble() const
    { 
        static const double two64 = double(1ull << 32)* double(1ull << 32); //2^64
        double d = hi * two64 + lo;
        return isPositive ? d : -d;    
    }

    /*mpz_class toMpz() const
    {
        mpz_class x = data[3];
        x = (x << 32) + data[2];
        x = (x << 32) + data[1];
        x = (x << 32) + data[0];
        return (isPositive) ? x : mpz_class(-x);
    }*/

    explicit operator int64_t() const
    {
        assert(hi == 0 && "Overflow");
        assert( (lo & 0x8000000000000000ull) == 0 && "Overflow"); //the int64_t needs the highest-order bit as its 'sign' bit

        return isPositive ? lo : (~lo)+1;
    }

    explicit operator uint64_t() const
    {
        assert(hi == 0 && "Overflow");
        assert( isPositive && "Overflow (negative number)");
        return lo;
    }
    
    int128_t& operator*=(int128_t other)
    {
        *this = *this * other;
        return *this;
    }


    explicit operator int32_t() const
    {
        assert(hi == 0 && (lo & 0xFFFFFFFF80000000ull == 0) && "Overflow"); //the int64_t needs the highest-order bit as its 'sign' bit
        return isPositive ? lo : (~lo)+1;
    }

    explicit operator uint32_t() const
    {
        assert(hi == 0 && (lo && 0xFFFFFFFF00000000ull == 0) && "Overflow");
        return lo;
    }

private:
    int128_t(bool pIsPositive, uint64_t pHi, uint64_t pLo): isPositive(pIsPositive), hi(pHi), lo(pLo) { }
    
    friend int128_t operator+ (int128_t a, int128_t b);
    friend int128_t operator- (int128_t a, int128_t b);
    friend int128_t operator* (int128_t a, int128_t b);
    friend int128_t operator* (int128_t a, uint64_t b);

    
    friend int128_t operator/ (int128_t a, uint64_t b);
    friend int128_t operator% (int128_t a, uint64_t b);
    friend int128_t operator/ (int128_t a, int128_t b);
    friend int128_t operator% (int128_t a, int128_t b);

    friend int128_t operator<<(int128_t a, uint32_t i);
    friend int128_t operator>>(int128_t a, uint32_t i);

    friend int128_t operator| (const int128_t a, const int128_t b);

    friend bool operator==(int128_t a, int128_t b);
    friend bool operator!=(int128_t a, int128_t b);
    friend bool operator< (const int128_t a, const int128_t b);
    friend bool operator<=(const int128_t a, const int128_t b);

    friend bool operator> (const int128_t a, const int128_t b);
    friend bool operator>=(const int128_t a, const int128_t b);

    friend int128_t divmod (const int128_t a, const uint64_t b, uint64_t &mod);
    friend int128_t divmod (int128_t a, int128_t b, int128_t &mod);

    friend int128_t abs(int128_t a);


    friend std::ostream& operator<<(std::ostream &os, int128_t a);

    //static const int128_t zero(true, 0, 0, 0, 0);
private:
    bool isPositive;
    uint64_t hi,lo;
};





#endif
