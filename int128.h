
#ifndef INT128_H
#define INT128_H

#include <stdint.h>
#include <assert.h>

#include <gmpxx.h>

#include <iostream>

class int128_t
{
public:
    int128_t() {data[0] = data[1] = data[2] = data[3] = 0; isPositive = true;}
    int128_t(uint32_t a) { data[0] = a; data[1] = data[2] = data[3] = 0; isPositive = true;}
    int128_t(int32_t a) 
    { 
        data[3] = data[2] = data[1] = 0; 
        isPositive = ! (a & 0x80000000); // sign bit is set

        data[0] = isPositive ? a : ((~a) +1); //two's complement; +1 cannot overflow since we have an additional bit (the former sign bit) available
    }

    int128_t(int64_t a) 
    { 
        data[3] = data[2] = 0; 
        isPositive = ! (a & 0x8000000000000000ull);
        
        if (!isPositive) a= (~a)+1; //two's complement

        data[0] =  a & 0x00000000FFFFFFFF;
        data[1] = (a & 0xFFFFFFFF00000000ll) >> 32;
    }

    int128_t(uint64_t a): isPositive(true)
    { 
        data[3] = data[2] = 0; 
        data[0] =  a & 0x00000000FFFFFFFF;
        data[1] = (a & 0xFFFFFFFF00000000ll) >> 32;
    }

 
    int128_t operator-() const { return int128_t(!isPositive, data[3], data[2], data[1], data[0]); }

    

    double toDouble() const
    { 
        double d = data[3];
        d = (d * (1ull << 32) + data[2]);
        d = (d * (1ull << 32) + data[1]);
        d = (d * (1ull << 32) + data[0]);
        return isPositive ? d : -d;    
    }

    mpz_class toMpz() const
    {
        mpz_class x = data[3];
        x = (x << 32) + data[2];
        x = (x << 32) + data[1];
        x = (x << 32) + data[0];
        return (isPositive) ? x : mpz_class(-x);
    }

    explicit operator int64_t() const
    {
        assert(data[3] == 0 && data[2] == 0 && "Overflow");
        assert( (data[1] & 0x80000000) == 0 && "Overflow"); //the int64_t needs the highest-order bit as its 'sign' bit

        uint64_t a = ((uint64_t)data[1]) << 32 | data[0];
        return isPositive ? a : (~a)+1;
    }

    explicit operator uint64_t() const
    {
        assert(data[3] == 0 && data[2] == 0 && "Overflow");
        return ((uint64_t)data[1]) << 32 | data[0];
    }
    
    int128_t& operator*=(int128_t other)
    {
        *this = *this * other;
        return *this;
    }


    explicit operator int32_t() const
    {
        assert(data[3] == 0 && data[2] == 0 && data[1] == 0 && "Overflow");
        assert( (data[0] & 0x80000000) == 0 && "Overflow"); //the int64_t needs the highest-order bit as its 'sign' bit

        return isPositive ? data[0] : (~data[0])+1;
    }

    explicit operator uint32_t() const
    {
        assert(data[3] == 0 && data[2] == 0 && data[1] == 0 && "Overflow");
        return data[0];
    }

private:
    int128_t(bool pIsPositive, uint32_t d3, uint32_t d2, uint32_t d1, uint32_t d0): 
        isPositive(pIsPositive) { data[0] = d0; data[1] = d1; data[2] = d2; data[3] = d3;}
    
    friend int128_t operator+ (int128_t a, int128_t b);
    friend int128_t operator- (int128_t a, int128_t b);
    friend int128_t operator* (int128_t a, int128_t b);
    friend int128_t operator* (int128_t a, uint32_t b);

    
    friend int128_t operator/ (int128_t a, uint32_t b);
    friend int128_t operator% (int128_t a, uint32_t b);
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

    friend int128_t divmod (const int128_t a, const uint32_t b, int64_t &mod);
    friend int128_t divmod (int128_t a, int128_t b, int128_t &mod);

    friend int128_t abs(int128_t a);


    friend std::ostream& operator<<(std::ostream &os, int128_t a);

    //static const int128_t zero(true, 0, 0, 0, 0);
private:
    bool isPositive;
    uint32_t data[4];
};





#endif
