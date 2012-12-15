
#include "int128.h"


bool operator<(const int128_t a, const int128_t b)
{
    //special case: both are zero --> none is smaller, no matter what their (possibly different) signs may be
    if ( a.data[3] == 0 && a.data[2] == 0 && a.data[1] == 0 && a.data[0] == 0 &&
         b.data[3] == 0 && b.data[2] == 0 && b.data[1] == 0 && b.data[0] == 0)
        return false;

    if (a.isPositive != b.isPositive)
        return b.isPositive;
        
    bool flipSign = (! a.isPositive);
    
    //special case: both values are equal--> none is smaller
    if (a.data[3] == b.data[3] && a.data[2] == b.data[2] &&
        a.data[1] == b.data[1] && a.data[0] == b.data[0])
        return false;
    
    bool sign = a.data[3] != b.data[3] ? a.data[3] < b.data[3] :
                a.data[2] != b.data[2] ? a.data[2] < b.data[2] :
                a.data[1] != b.data[1] ? a.data[1] < b.data[1] :
                a.data[0] <  b.data[0];
    return flipSign ? !sign : sign;
}

bool operator>(const int128_t a,  const int128_t b) { return b < a; }

bool operator<=(const int128_t a, const int128_t b) { return a < b || a == b;}
bool operator>=(const int128_t a, const int128_t b) { return a > b || a == b;}
 

bool operator==(int128_t a, int128_t b)
{
    return (a.isPositive == b.isPositive && a.data[0] == b.data[0] && a.data[1] == b.data[1] &&
            a.data[2] == b.data[2] && a.data[3] == b.data[3]);
}

bool operator!=(int128_t a, int128_t b)
{
    return ! (a==b);
}


int128_t operator+(int128_t a, int128_t b)
{
    
    if (a.isPositive == b.isPositive)
    {
        int128_t tmp;
        tmp.isPositive = a.isPositive;
        /*
        uint32_t carry = 0;
        
        for (int i = 0; i < 4; i++)
        {
            uint64_t sum = ((uint64_t)a.data[i]) + b.data[i] + carry;
            tmp.data[i] = sum & 0x00000000FFFFFFFFull;
            carry       =(sum & 0xFFFFFFFF00000000ull) >> 32;
        }*/
        uint64_t sum = ((uint64_t)a.data[0]) + b.data[0];
        tmp.data[0] = sum & 0x00000000FFFFFFFFull;

        sum         =(sum & 0xFFFFFFFF00000000ull) >> 32;
        sum +=       ((uint64_t)a.data[1]) + b.data[1];
        tmp.data[1] = sum & 0x00000000FFFFFFFFull;

        sum         =(sum & 0xFFFFFFFF00000000ull) >> 32;
        sum +=       ((uint64_t)a.data[2]) + b.data[2];
        tmp.data[2] = sum & 0x00000000FFFFFFFFull;

        sum         =(sum & 0xFFFFFFFF00000000ull) >> 32;
        sum +=       ((uint64_t)a.data[3]) + b.data[3];
        tmp.data[3] = sum & 0x00000000FFFFFFFFull;

        assert( (sum & 0xFFFFFFFF00000000ull) >> 32 == 0 && "Overflow");

        return tmp;

    } else if (a.isPositive)
        return a - (-b);
    else
        return b - (-a);
}


int128_t operator*(int128_t a, uint32_t b)
{
    uint32_t carry = 0;
    for (int i = 0; i < 4; i++)
    {
        uint64_t prod = ((uint64_t)a.data[i]) * b + carry;
        
        a.data[i] = prod & 0x00000000FFFFFFFFull;
        carry     =(prod & 0xFFFFFFFF00000000ull) >> 32;
    }
    assert (carry == 0 && "Overflow");
    return a;
}


int128_t operator*(int128_t a, int128_t b)
{

    int128_t res;
    res.isPositive = (a.isPositive == b.isPositive);

    /*    a3    a2    a1    a0 * b3    b2    b1    b0
     *    ===========================================
     *                       | a3b0  a2b0  a1b0  a0b0
     *                   a3b1| a2b1  a1b1  a0b1
     *             a3b2  a2b2| a1b2  a0b2
     *       a3b3  a2b3  a1b3| a0b3                 
                 Overflow    |  fits into an int128_t  */
            
    assert( (a.data[3] == 0 || (b.data[3] == 0 && b.data[2] == 0 && b.data[1] == 0) ) && "Overflow");
    assert( (a.data[2] == 0 || (b.data[3] == 0 && b.data[2] == 0)                   ) && "Overflow");
    assert( (a.data[1] == 0 ||  b.data[3] == 0                                      ) && "Overflow");
     
    uint64_t sum = (uint64_t)a.data[0] * b.data[0];

    res.data[0] =  sum & 0x00000000FFFFFFFFull;
    uint64_t carry=(sum & 0xFFFFFFFF00000000ull) >> 32;

    //need to "shave off" the carry after each multiplication, otherwise 'sum' could overflow
    sum = (uint64_t)a.data[1]*b.data[0]+carry;
    carry = (sum & 0xFFFFFFFF00000000ull) >> 32;
    sum&= 0x00000000FFFFFFFFull;
    sum+= (uint64_t)a.data[0]*b.data[1];
    carry+= (sum & 0xFFFFFFFF00000000ull) >> 32;
    res.data[1] = sum & 0x00000000FFFFFFFFull;

    sum = (uint64_t)a.data[2]*b.data[0] + carry;
    carry = (sum & 0xFFFFFFFF00000000ull) >> 32;
    sum&= 0x00000000FFFFFFFFull;
    sum+= (uint64_t)a.data[1]*b.data[1];
    carry+= (sum & 0xFFFFFFFF00000000ull) >> 32;
    sum&= 0x00000000FFFFFFFFull;
    sum+= (uint64_t)a.data[0]*b.data[2];
    carry+= (sum & 0xFFFFFFFF00000000ull) >> 32;
    res.data[2] = sum & 0x00000000FFFFFFFFull;
    
    sum = (uint64_t)a.data[3]*b.data[0] + carry;
    carry = (sum & 0xFFFFFFFF00000000ull) >> 32;
    sum&= 0x00000000FFFFFFFFull;
    sum+= (uint64_t)a.data[2]*b.data[1];
    carry+= (sum & 0xFFFFFFFF00000000ull) >> 32;
    sum&= 0x00000000FFFFFFFFull;
    sum+= (uint64_t)a.data[1]*b.data[2];
    carry+= (sum & 0xFFFFFFFF00000000ull) >> 32;
    sum&= 0x00000000FFFFFFFFull;
    sum+= (uint64_t)a.data[0]*b.data[3];
    carry+= (sum & 0xFFFFFFFF00000000ull) >> 32;
    res.data[3] = sum & 0x00000000FFFFFFFFull;
    
    assert(carry == 0 && "Overflow");
    
    return res;
    
    /*int128_t res = a * b.data[0] + 
                  (a * b.data[1] << 32) + 
                  (a * b.data[2] << 64) + 
                  (a * b.data[2] << 96);
    return res; */
}

int128_t divmod (const int128_t a, const uint32_t b, uint32_t &mod)
{
    int128_t res;
    res.isPositive = a.isPositive;
    
    res.data[3] = a.data[3] / b;
    
    int64_t tmp = a.data[3] - (res.data[3] * (int64_t)b);
    tmp = (tmp << 32) | a.data[2];
    assert(tmp >= 0);
    
    res.data[2] = tmp / b;
    tmp = tmp - (res.data[2] * (int64_t)b);
    tmp = (tmp << 32) | a.data[1];
    assert( tmp >=0);
    
    res.data[1] = tmp / b;
    tmp = tmp - (res.data[1] * (int64_t)b);
    tmp = (tmp << 32) | a.data[0];
    assert( tmp >= 0);
    
    res.data[0] = tmp / b;
    tmp = tmp - (res.data[0] * (int64_t)b);
    assert( tmp < b);
    assert( res*b + tmp == a);
    //FIXME: round towards zero or up?
    
    mod = tmp;
    
    return res;

}

int128_t divmod (int128_t a, int128_t b, int128_t &res_mod)
{
    int128_t res;
    int128_t remainder;
    assert( b != 0);
    
    bool resNegative = (a.isPositive != b.isPositive);
    a.isPositive = b.isPositive = true;
        
    for (int idx = 3; idx >= 0; idx--)
    {
        uint32_t div = a.data[idx] / b.data[idx];
        if ( b*div > a ) div--;
        assert( b*div <= a);
        
        res = a /div;
        a = a - res*div;
        assert(a.data[idx] == 0);
    }

    assert ( a < b);
    
    res.isPositive = !resNegative;
    
    res_mod = (resNegative) ? -a : a;
    
    return res;
    /*
    if (remainder < 0) 
    {   
        remainder+= b;
        assert (remainder > 0)
        res -=1;
    }*/
    
}


int128_t operator/ (const int128_t a, const uint32_t b)
{
    uint32_t dummy;
    return divmod(a, b, dummy);
}


int128_t operator-(int128_t a, int128_t b)
{
    
    if (a.isPositive == b.isPositive)
    {
        int128_t res;
        res.isPositive = (b <= a);
        a.isPositive = b.isPositive = true; //HACK: so that the next line computes the relation of the *absolute* values
        if (b > a)
        {
            int128_t tmp = b;
            b = a;
            a = tmp;
        }
        
        assert( b <= a);
        
        /*
        uint32_t carry = 0;
        //uint32_t res[4];
        for (int i = 0; i < 4; i++)
        {
            int64_t diff = ((int64_t)a.data[i]) - b.data[i] - carry;

            carry = (diff < 0) ? 1 : 0;
            //as a (necessary) side effect, this assignment of an int64_t to an uint32_t 'repairs' the potentially negative "diff"
            res.data[i] = diff; 
        }*/

        int64_t diff;
        uint32_t carry;
        diff = ((int64_t)a.data[0]) - b.data[0];
        carry = (diff < 0);
        res.data[0] = diff;

        diff = ((int64_t)a.data[1]) - b.data[1] - carry;
        carry = (diff < 0);
        res.data[1] = diff;
        
        diff = ((int64_t)a.data[2]) - b.data[2] - carry;
        carry = (diff < 0);
        res.data[2] = diff;

        diff = ((int64_t)a.data[3]) - b.data[3] - carry;
        carry = (diff < 0);
        res.data[3] = diff;
            
        assert( carry == 0 && "buggy arithmetic" );
        return res;

    } else if (a.isPositive) //'b' negative
        return a + (-b);    
    else //'a' negative
        return - ( (-a) + b );
}

int128_t operator| (int128_t a, int128_t b)
{
    assert(a.isPositive == b.isPositive && "Opposite signs in OR, undefined result");
    return int128_t(a.isPositive, a.data[3] | b.data[3], a.data[2] | b.data[2], a.data[1] | b.data[1], a.data[0] | b.data[0]);
}

int128_t abs(int128_t a)
{
    a.isPositive = true;
    return a;
}


int128_t operator<<(int128_t a, uint32_t i)
{
    /*WARNING: on x86 the shift operations use the ASM operation 'shl' internally.
     *         shl shifts not by 'i', but by 'i % 32'
     *         thus, 'x << 32' on an uint32_t does not return zero, but instead returns x unchanged
     *         Therefore, all shifts of - AND INCLUDING - 32 and greater have to be handled seperately
     */
    if (a == int128_t(0) ) return a;
    
    assert ( i < 128);  //would overflow in any case
    
    while (i >= 32) //shift by a whole uint32_t
    {
        assert( a.data[3] == 0 && "Overflow");
        a.data[3] = a.data[2];
        a.data[2] = a.data[1];
        a.data[1] = a.data[0];
        a.data[0] = 0;
        i-=32;
    }

    /* WARNING: needs to early-terminate here, because '>>' is also modulo 32, so x>>32 returns x instead of 0**/
    if (i == 0) return a;
    
    assert( (a.data[3] >> (32- i)) == 0 && "Overflow");
    
    a.data[3] = (a.data[3] << i) |  a.data[2] >> (32-i);
    a.data[2] = (a.data[2] << i) | (a.data[1] >> (32-i));
    a.data[1] = (a.data[1] << i) | (a.data[0] >> (32-i));
    a.data[0] =  a.data[0] << i;
    return a;
}

std::ostream& operator<<(std::ostream &os, int128_t a)
{
#ifdef HEX_MODE
    static const char* hex_digits = "0123456789ABCDEF";
    
    if (!a.isPositive) 
        os << "(-)";
    os << "0x";
        
    for (int i = 3; i >=0; i--)
    {
        uint32_t shift = 28;
        for (uint32_t mask = 0xF0000000; mask; mask >>= 4, shift -= 4)
        {
            uint32_t nibble = (a.data[i] & mask) >> shift;
            assert (nibble < 16);
            os << hex_digits[nibble];
        }
    }
#else
    os << a.toMpz();
 
#endif
    return os;
}
