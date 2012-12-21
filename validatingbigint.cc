
#include "validatingbigint.h"


ValidatingBigint operator+ (ValidatingBigint a, ValidatingBigint b) { return ValidatingBigint(a.i+b.i, a.mpz+b.mpz);}
ValidatingBigint operator- (ValidatingBigint a, ValidatingBigint b) { return ValidatingBigint(a.i-b.i, a.mpz-b.mpz);}
ValidatingBigint operator* (ValidatingBigint a, ValidatingBigint b) { return ValidatingBigint(a.i*b.i, a.mpz*b.mpz);}
ValidatingBigint operator/ (ValidatingBigint a, uint32_t b)         { return ValidatingBigint(a.i/b,   a.mpz/b);}
ValidatingBigint operator% (ValidatingBigint a, uint32_t b)         { return ValidatingBigint(a.i%b,   a.mpz%b);}
ValidatingBigint operator/ (ValidatingBigint a, ValidatingBigint b) { return ValidatingBigint(a.i/b.i, a.mpz/b.mpz);}
ValidatingBigint operator% (ValidatingBigint a, ValidatingBigint b) { return ValidatingBigint(a.i%b.i, a.mpz%b.mpz);}


ValidatingBigint operator<<(ValidatingBigint a, uint32_t i)         { return ValidatingBigint(a.i<<i, a.mpz<<i);}

ValidatingBigint operator| (ValidatingBigint a, ValidatingBigint b) { return ValidatingBigint(a.i|b.i, a.mpz|b.mpz);};

bool operator==(ValidatingBigint a, ValidatingBigint b)
{
    assert(  (a.i == b.i) == (a.mpz == b.mpz));
    return a.i == b.i;
};

bool operator!=(ValidatingBigint a, ValidatingBigint b)
{
    assert( (a.i != b.i) == (a.mpz != b.mpz));
    return a.i != b.i;
}

bool operator<=(const ValidatingBigint a, const ValidatingBigint b)
{
    assert( (a.i <= b.i) == (a.mpz <= b.mpz));
    return a.i <= b.i;
}

bool operator<(const ValidatingBigint a, const ValidatingBigint b)
{
    assert(  (a.i < b.i) == (a.mpz < b.mpz));
    return a.i < b.i;
};

bool operator>(const ValidatingBigint a, const ValidatingBigint b)
{
    assert(  (a.i > b.i) == (a.mpz > b.mpz));
    return a.i > b.i;
};

bool operator>=(const ValidatingBigint a, const ValidatingBigint b)
{
    assert(  (a.i >= b.i) == (a.mpz >= b.mpz));
    return a.i >= b.i;
};

ValidatingBigint abs(ValidatingBigint a)
{
    return ValidatingBigint( abs(a.i), abs(a.mpz));
}

std::ostream& operator<<(std::ostream &os, ValidatingBigint a)
{
    os << a.mpz;
    return os;
}

