
#ifndef FRACTION_H
#define FRACTION_H

#include <iostream>

template<typename int_type>
class Fraction
{
public:
    Fraction(const int_type p_num = 0, const int_type p_denom = 1): num(p_num), denom(p_denom) 
    {
        assert( p_denom != 0);
        if (p_denom < 0)
        {
            num = -num;
            denom = -denom;
        }
    }

   
    Fraction& operator+=(const Fraction &other)
    {
        num = num*other.denom + denom*other.num;
        denom = denom*other.denom;
        return *this;
    }

    
    Fraction& operator-=(const Fraction &other)
    {
        num = num*other.denom - denom*other.num;
        denom = denom*other.denom;
        return *this;
    }

    
    Fraction& operator*=(const Fraction &other)
    {
        num*=other.num;
        denom*=other.denom;
        return *this;
    }
    
  
    Fraction& operator/=(const Fraction &other)
    {
        num*=other.denom;
        denom*=other.num;
        return *this;
    }
   
    static int_type gcd(int_type u, int_type v)
    {
    
      // simple cases (termination)
      if (u == v)
        return u;
      if (u == 0)
        return v;
      if (v == 0)
        return u;
     
      // look for factors of 2
      if (u % 2 == 0) // u is even
        return (v % 2 == 1) ? gcd(u >> 1, v) : gcd(u >> 1, v >> 1) << 1;

      if (v % 2 == 0) // u is odd, v is even
        return gcd(u, v >> 1);
     
      // reduce larger argument
      if (u > v)
        return gcd((u - v) >> 1, v);
      return gcd((v - u) >> 1, u);
    }
    
    Fraction reduced()
    {
        int_type g = gcd(num, denom);
        return Fraction( num/g, denom/g );
    }
    
    bool operator < (const Fraction &other) const { return num * other.denom <  other.num * denom; }
    bool operator <=(const Fraction &other) const { return num * other.denom <= other.num * denom; }

    bool operator > (const Fraction &other) const { return num * other.denom >  other.num * denom; }
    bool operator >=(const Fraction &other) const { return num * other.denom >= other.num * denom; }

    bool operator ==(const Fraction &other) const { return num * other.denom == other.num * denom; }
    bool operator !=(const Fraction &other) const { return num * other.denom != other.num * denom; }

    Fraction operator+(const Fraction &other) const { Fraction res = *this; return res+=other; }
    Fraction operator-(const Fraction &other) const { Fraction res = *this; return res-=other; }
    Fraction operator*(const Fraction &other) const { Fraction res = *this; return res*=other; }
    Fraction operator/(const Fraction &other) const { Fraction res = *this; return res/=other; }
    
    int_type get_num()   const { return num; }
    int_type get_denom() const { return denom; }
private:
 int_type num, denom;

};

template<typename int_type> 
ostream & operator<<(ostream &out, const Fraction<int_type> &val)
{
    std::cout << "(" << val.get_num() << "/" << val.get_denom() << ")";
    return out;
}

#endif

