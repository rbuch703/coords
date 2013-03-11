
#include "int128ng.h"

// from math64.asm
extern "C" uint64_t add (uint64_t a, uint64_t b, uint64_t *carry_out);
extern "C" uint64_t sub (uint64_t a, uint64_t b, uint64_t *borrow);
extern "C" uint64_t add3(uint64_t a, uint64_t b, uint64_t c, uint64_t *carry_out);
extern "C" uint64_t sub128(uint64_t a_hi, uint64_t a_lo, uint64_t b_hi, uint64_t b_lo, uint64_t *res_hi, uint64_t *res_lo);
extern "C" uint64_t mul (uint64_t a, uint64_t b, uint64_t *hi);
extern "C" uint64_t div128(uint64_t *a_hi, uint64_t *a_lo, uint64_t b);


bool operator<(const int128_t a, const int128_t b)
{
    //special case: both are zero --> none is smaller, no matter what their (possibly different) signs may be
    if ( a.hi == 0 && a.lo == 0 && b.hi == 0 && b.lo == 0) return false;

    if (a.isPositive != b.isPositive) return b.isPositive;
    
    //if this point is reached, 'a' and 'b' have equal sign
    bool flipSign = (! a.isPositive);
    
    //special case: both values are equal--> none is smaller, no matter the actual sign
    if (a.hi == b.hi && a.lo == b.lo) return false;
    
    bool sign = a.hi != b.hi ? a.hi < b.hi :
                a.lo < b.lo;
                
    return flipSign ? !sign : sign;
}

bool operator>(const int128_t a,  const int128_t b) { return b < a; }

bool operator<=(const int128_t a, const int128_t b) { return a < b || a == b;}
bool operator>=(const int128_t a, const int128_t b) { return a > b || a == b;}
 

bool operator==(int128_t a, int128_t b)
{
    //if both are zero, they are equal, no matter their sign
    if ( a.hi == 0 && a.lo == 0 && b.hi == 0 && b.lo == 0)
         return true;

    return (a.isPositive == b.isPositive && a.hi == b.hi && a.lo == b.lo);
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
        
        uint64_t carry;
        tmp.lo = add(a.lo, b.lo, &carry);
        tmp.hi = add(a.hi, carry,&carry);
        
        assert (!carry && "Overflow");
        tmp.hi = add(tmp.hi, b.hi, &carry);
        assert (!carry && "Overflow");
        
        return tmp;

    } else if (a.isPositive)
        return a - (-b);
    else
        return b - (-a);
}

int128_t operator*(int128_t a, uint64_t b)
{

    uint64_t hi1;
    uint64_t lo = mul(a.lo, b, &hi1);
    uint64_t overflow;
    uint64_t hi = mul(a.hi, b, &overflow);
    assert(!overflow && "Overflow");
    
    hi = add(hi, hi1, &overflow);
    assert(!overflow && "Overflow");
    
    return int128_t( a.isPositive, hi, lo);
}

/*int128_t operator*(int128_t a, uint32_t b)
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
}*/


int128_t operator*(int128_t a, int128_t b)
{

    
    bool isPositive = (a.isPositive == b.isPositive);
    
    uint64_t hi;
    uint64_t lo = mul(a.lo, b.lo, /*out*/&hi);

    uint64_t overflow;
    uint64_t hi2 = mul(a.lo, b.hi, /*out*/&overflow);
    assert( !overflow );

    uint64_t hi3 = mul(a.hi, b.lo, /*out*/&overflow);
    assert( !overflow);
    
    assert ((a.hi == 0 || b.hi == 0) && "Overflow");
    hi = add3( hi, hi2, hi3, &overflow);
    assert(!overflow);
    
    return int128_t(isPositive, hi, lo);
}

/* since b is in range [0, 2^32-1], mod can be in range [-2^32-1, 2^32-1] (the former only if a is negative),
   and thus we need something bigger than an int32_t to hold 'b' */

int128_t divmod (const int128_t a, const uint64_t b, uint64_t &mod)
{
    int128_t res = a;
    
    mod = div128(&res.hi, &res.lo, b);

    return res;

}

/* determines the length of the sequence of zero-valued high-order bits in 'a' */
inline int numHighZero(uint64_t a)
{
    assert ( a!= 0); // if 'a' is zero, this method should not need to be used
    int shl = 0;
    if (! (a & 0xFFFFFFFF00000000ull)) { shl += 32; a <<= 32;}    
    if (! (a & 0xFFFF000000000000ull)) { shl += 16; a <<= 16;}
    if (! (a & 0xFF00000000000000ull)) { shl += 8;  a <<= 8;}
    if (! (a & 0xF000000000000000ull)) { shl += 4;  a <<= 4;}
    if (! (a & 0xC000000000000000ull)) { shl += 2;  a <<= 2;}
    if (! (a & 0x8000000000000000ull)) { shl += 1;  a <<= 1;}
    
    assert(shl <= 64);
    return shl;
}

int128_t divmod (int128_t a, int128_t b, int128_t &res_mod)
{
    if (b.hi == 0) 
    { 
        res_mod = div128(&a.hi, &a.lo, b.lo);//divmod(a, b.lo, res_mod);
        res_mod.isPositive = a.isPositive;
        a.isPositive = (a.isPositive == b.isPositive);
        return a;
    }

    if ( abs(b) > abs(a) )
    {
        res_mod = a;
        return 0;
    }
    
    if (abs(b) == abs(a))
    {
        res_mod = 0;
        return int128_t( a.isPositive == b.isPositive, 0, 1);
    }

    int128_t remainder;
    assert( b != 0);

    bool aPositive = a.isPositive;
    bool resPositive = (a.isPositive == b.isPositive);
    
    a.isPositive = b.isPositive = true;

    //int b_idx = b.hi ? 1 : 0;
    
    //int128_t res(0);

    assert(b.hi < 0xFFFFFFFFFFFFFFFFull && "not implemented"); // otherwise we could not increase b_hi by 1
    
    
    /* if 'b' is small (i.e. b.hi has many leading zeros) the relative difference between 'b' and '(b.hi << 64)'
       can become very big. Thus, in the naive approach of computing '(a/b.hi) >> 64' as an approximation to 'a/b',
       a high error introduced. Thus, the while loop would need to perform many iterations in order to compensate.
       Instead, we fill up the leading zeroes by left-shifting 'b' and then undo the left-shift by later adjusting
       the '>> 64' correspondingly.
     */
    int b_shl = numHighZero(b.hi);
    uint64_t b_hi_tmp = b_shl == 0 ? b.hi : (b.hi << b_shl) | (b.lo >> (64 - b_shl));
    

    int128_t div_hi = (a/b_hi_tmp);
    div_hi = div_hi >> (64 - b_shl);    //seems to trigger some kind of bug when integrated into the line above
    int128_t div_lo = (a/(b_hi_tmp+1));
    div_lo = div_lo >> (64 - b_shl);

    /*int128_t div_hi = (a/b.hi) >> 64;
    int128_t div_lo = a/(b.hi+1) >> 64;*/
    //if (div_hi > div_lo+1)
    //    std::cout << "difference is " << (div_hi - div_lo) << std::endl;
        
    while (div_hi != div_lo)
    {
        assert( div_hi > div_lo);
        int128_t mid = (div_hi + div_lo)/2;
        
        int128_t prod( b*mid );
        if (prod <= a)
        {
            if (prod + b > a)
                { div_hi = div_lo =mid; break; } //found the right divisor 
            else  //have to go on looking, but 'mid' is too low
                div_lo = mid + 1;
        }
        else
        {
            assert( prod > a);
            div_hi = mid-1;
        }
    }
    
    a = a - b * div_hi;
    
    assert ( a < b);
    
    res_mod = (aPositive) ? a : -a;
    
    return resPositive ? div_hi : -div_hi;
}

int128_t operator/ (int128_t a, int128_t b)
{
    int128_t dummy;
    return divmod(a, b, dummy);
}

int128_t operator% (int128_t a, int128_t b)
{
    int128_t remainder;
    divmod(a,b,remainder);
    return remainder;
}


int128_t operator/ (int128_t a, const uint64_t b)
{
    uint64_t dummy;
    return divmod(a, b, dummy);
}

int128_t operator% (int128_t a, uint64_t b)
{
    uint64_t remainder;
    divmod(a,b,remainder);
    return remainder;
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
        
        //int64_t diff;
#ifndef NDEBUG
        uint64_t carry = 
#endif
        sub128(a.hi, a.lo, b.hi, b.lo, &res.hi, &res.lo);
                    
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
    return int128_t(a.isPositive, a.hi | b.hi, a.lo | b.lo);
}

/* //DO NOT USE, is not well-defined since int128_t is not in two's complement
int128_t operator~ (int128_t a)
{
    a.isPositive = ! a.isPositive;
    a.hi = ~a.hi;
    a.lo = ~a.lo;
    return a;
}*/



int128_t abs(int128_t a)
{
    a.isPositive = true;
    return a;
}


int128_t operator<<(int128_t a, uint32_t i)
{
    /*WARNING: on x86 the shift operations use the ASM operation 'shl' internally.
     *         shl shifts not by 'i', but by 'i % 64'
     *         thus, 'x << 64' on an uint64_t does not return zero, but instead returns x unchanged
     *         Therefore, all shifts of - AND INCLUDING - 64 and greater have to be handled seperately
     */
     
    //need this early termination, because 0 is the only value for which a left-shift by an arbitrary number does not overflow
    if (a.hi == 0 && a.lo == 0 ) return a;
    
    if (i >= 64)
    {
        assert( (a.hi==0) && "Overflow");
        a.hi = a.lo;
        a.lo = 0;
        i -= 64;
    }

    /* WARNING: needs to early-terminate here, because '>>' is also modulo 64, so x>>64 returns x instead of 0**/
    assert( i < 64 && "Overflow");
    
    if (i == 0) return a;
    
    assert (a.hi >> (64-i) == 0 && "Overflow");

    a.hi = (a.hi << i) | (a.lo >> (64-i));
    a.lo = a.lo << i;

    return a;
}


/* this method needs special treatment for negative nunbers. Due to the usual two's complement,
   a right-shift by 's' on negative numbers has the effect of dividing by 2^s and rounding down, i.e. towards negative infinity.
   Since the in128_t is not stored in two's complement, it would instead round towards zero.
   To arrive at correct arithmetic this rounding up effect need to be simulated. This is done by adding '1' to the final result
   of the shift whenever at least one bit that was shifted out was non-zero
*/
int128_t operator>>(int128_t a, uint32_t i)
{
    /*WARNING: on x86/x64 the shift operations use the ASM operation 'shr' internally.
     *         shr shifts are not done by 'i', but by 'i % 64'
     *         Therefore, all shifts of - AND INCLUDING - 64 and greater have to be handled seperately
     */
    if ((a.hi == 0 && a.lo == 0) || (i == 0)) return a;
    
    bool shiftedOutNonZero = false;
    if (i >= 64)    //shift by a whole uint64_t manually
    {
        shiftedOutNonZero = a.lo;
        a.lo = a.hi;
        a.hi = 0;
        i   -= 64;
    }
    
    if (i >= 64) return a.isPositive ? 0 : -1; //would shift out everything
    
    /* WARNING: needs to early-terminate here, because '<<' is also modulo 64, so x<<64 returns x instead of 0**/
    if (i == 0) return a.isPositive | !shiftedOutNonZero ? a : a-1;

    shiftedOutNonZero |= a.lo << (64-i);
    a.lo = (a.lo >> i) | (a.hi << (64-i));
    a.hi = (a.hi >> i);

    return a.isPositive | !shiftedOutNonZero ? a : a-1;
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
        uint64_t shift = 64 - 4;
        for (uint64_t mask = 0xF000000000000000ull; mask; mask >>= 4, shift -= 4)
        {
            uint64_t nibble = (a.data[i] & mask) >> shift;
            assert (nibble < 16);
            os << hex_digits[nibble];
        }
    }
#else
    //if (a.hi == 0)
        os << (a.isPositive ? "" : "-") << a.lo;
    //else
    //    os << a.toDouble();
 
#endif
    return os;
}
