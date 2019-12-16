// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The PaydayCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAYDAYCOIN_UINT256_H
#define PAYDAYCOIN_UINT256_H

#include <assert.h>
#include <cstring>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

/** Template base class for fixed-sized opaque blobs. */
template<unsigned int BITS>
class base_blob
{
public:
    static constexpr int WIDTH = BITS / 8;
    uint8_t data[WIDTH];
public:
    base_blob()
    {
        memset(data, 0, sizeof(data));
    }

    explicit base_blob(const std::vector<unsigned char>& vch);

    bool IsNull() const
    {
        for (int i = 0; i < WIDTH; i++)
            if (data[i] != 0)
                return false;
        return true;
    }

    void SetNull()
    {
        memset(data, 0, sizeof(data));
    }

    inline int Compare(const base_blob& other) const { return memcmp(data, other.data, sizeof(data)); }

    friend inline bool operator==(const base_blob& a, const base_blob& b) { return a.Compare(b) == 0; }
    friend inline bool operator!=(const base_blob& a, const base_blob& b) { return a.Compare(b) != 0; }
    friend inline bool operator<(const base_blob& a, const base_blob& b) { return a.Compare(b) < 0; }

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);
    std::string ToString() const;

    unsigned char* begin()
    {
        return &data[0];
    }

    unsigned char* end()
    {
        return &data[WIDTH];
    }

    const unsigned char* begin() const
    {
        return &data[0];
    }

    const unsigned char* end() const
    {
        return &data[WIDTH];
    }

    unsigned int size() const
    {
        return sizeof(data);
    }

    uint64_t GetUint64(int pos) const
    {
        const uint8_t* ptr = data + pos * 8;
        return ((uint64_t)ptr[0]) | \
               ((uint64_t)ptr[1]) << 8 | \
               ((uint64_t)ptr[2]) << 16 | \
               ((uint64_t)ptr[3]) << 24 | \
               ((uint64_t)ptr[4]) << 32 | \
               ((uint64_t)ptr[5]) << 40 | \
               ((uint64_t)ptr[6]) << 48 | \
               ((uint64_t)ptr[7]) << 56;
    }

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        s.write((char*)data, sizeof(data));
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        s.read((char*)data, sizeof(data));
    }
};

/** 160-bit opaque blob.
 * @note This type is called uint160 for historical reasons only. It is an opaque
 * blob of 160 bits and has no integer operations.
 */
class uint160 : public base_blob<160> {
public:
    uint160() {}
    explicit uint160(const std::vector<unsigned char>& vch) : base_blob<160>(vch) {}
};

/** 256-bit opaque blob.
 * @note This type is called uint256 for historical reasons only. It is an
 * opaque blob of 256 bits and has no integer operations. Use arith_uint256 if
 * those are required.
 */
class uint256 : public base_blob<256> {
public:
    uint256() {}
    explicit uint256(const std::vector<unsigned char>& vch) : base_blob<256>(vch) {}
};

/* uint256 from const char *.
 * This is a separate function because the constructor uint256(const char*) can result
 * in dangerously catching uint256(0).
 */
inline uint256 uint256S(const char *str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}
/* uint256 from std::string.
 * This is a separate function because the constructor uint256(const std::string &str) can result
 * in dangerously catching uint256(0) via std::string(const char*).
 */
inline uint256 uint256S(const std::string& str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}

/** Template base class for unsigned big integers. */
template <unsigned int BITS>
class base_uint_pday
{
protected:
    enum { WIDTH = BITS / 32 };
    uint32_t pn[WIDTH];

public:
    base_uint_pday()
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = 0;
    }

    base_uint_pday(const base_uint_pday& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
    }


    bool IsNull() const
    {
        for (int i = 0; i < WIDTH; i++)
            if (pn[i] != 0)
                return false;
        return true;
    }

    void SetNull()
    {
        memset(pn, 0, sizeof(pn));
    }


    base_uint_pday& operator=(const base_uint_pday& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
        return *this;
    }

    base_uint_pday(uint64_t b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
    }

    explicit base_uint_pday(const std::string& str);
    explicit base_uint_pday(const std::vector<unsigned char>& vch);

    bool operator!() const
    {
        for (int i = 0; i < WIDTH; i++)
            if (pn[i] != 0)
                return false;
        return true;
    }

    const base_uint_pday operator~() const
    {
        base_uint_pday ret;
        for (int i = 0; i < WIDTH; i++)
            ret.pn[i] = ~pn[i];
        return ret;
    }

    const base_uint_pday operator-() const
    {
        base_uint_pday ret;
        for (int i = 0; i < WIDTH; i++)
            ret.pn[i] = ~pn[i];
        ret++;
        return ret;
    }

    double getdouble() const;

    base_uint_pday& operator=(uint64_t b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
        return *this;
    }

    base_uint_pday& operator^=(const base_uint_pday& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] ^= b.pn[i];
        return *this;
    }

    base_uint_pday& operator&=(const base_uint_pday& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] &= b.pn[i];
        return *this;
    }

    base_uint_pday& operator|=(const base_uint_pday& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] |= b.pn[i];
        return *this;
    }

    base_uint_pday& operator^=(uint64_t b)
    {
        pn[0] ^= (unsigned int)b;
        pn[1] ^= (unsigned int)(b >> 32);
        return *this;
    }

    base_uint_pday& operator|=(uint64_t b)
    {
        pn[0] |= (unsigned int)b;
        pn[1] |= (unsigned int)(b >> 32);
        return *this;
    }

    base_uint_pday& operator<<=(unsigned int shift);
    base_uint_pday& operator>>=(unsigned int shift);

    base_uint_pday& operator+=(const base_uint_pday& b)
    {
        uint64_t carry = 0;
        for (int i = 0; i < WIDTH; i++) {
            uint64_t n = carry + pn[i] + b.pn[i];
            pn[i] = n & 0xffffffff;
            carry = n >> 32;
        }
        return *this;
    }

    base_uint_pday& operator-=(const base_uint_pday& b)
    {
        *this += -b;
        return *this;
    }

    base_uint_pday& operator+=(uint64_t b64)
    {
        base_uint_pday b;
        b = b64;
        *this += b;
        return *this;
    }

    base_uint_pday& operator-=(uint64_t b64)
    {
        base_uint_pday b;
        b = b64;
        *this += -b;
        return *this;
    }

    base_uint_pday& operator*=(uint32_t b32);
    base_uint_pday& operator*=(const base_uint_pday& b);
    base_uint_pday& operator/=(const base_uint_pday& b);

    base_uint_pday& operator++()
    {
        // prefix operator
        int i = 0;
        while (++pn[i] == 0 && i < WIDTH - 1)
            i++;
        return *this;
    }

    const base_uint_pday operator++(int)
    {
        // postfix operator
        const base_uint_pday ret = *this;
        ++(*this);
        return ret;
    }

    base_uint_pday& operator--()
    {
        // prefix operator
        int i = 0;
        while (--pn[i] == (uint32_t)-1 && i < WIDTH - 1)
            i++;
        return *this;
    }

    const base_uint_pday operator--(int)
    {
        // postfix operator
        const base_uint_pday ret = *this;
        --(*this);
        return ret;
    }

    int CompareTo(const base_uint_pday& b) const;
    bool EqualTo(uint64_t b) const;

    friend inline const base_uint_pday operator+(const base_uint_pday& a, const base_uint_pday& b) { return base_uint_pday(a) += b; }
    friend inline const base_uint_pday operator-(const base_uint_pday& a, const base_uint_pday& b) { return base_uint_pday(a) -= b; }
    friend inline const base_uint_pday operator*(const base_uint_pday& a, const base_uint_pday& b) { return base_uint_pday(a) *= b; }
    friend inline const base_uint_pday operator/(const base_uint_pday& a, const base_uint_pday& b) { return base_uint_pday(a) /= b; }
    friend inline const base_uint_pday operator|(const base_uint_pday& a, const base_uint_pday& b) { return base_uint_pday(a) |= b; }
    friend inline const base_uint_pday operator&(const base_uint_pday& a, const base_uint_pday& b) { return base_uint_pday(a) &= b; }
    friend inline const base_uint_pday operator^(const base_uint_pday& a, const base_uint_pday& b) { return base_uint_pday(a) ^= b; }
    friend inline const base_uint_pday operator>>(const base_uint_pday& a, int shift) { return base_uint_pday(a) >>= shift; }
    friend inline const base_uint_pday operator<<(const base_uint_pday& a, int shift) { return base_uint_pday(a) <<= shift; }
    friend inline const base_uint_pday operator*(const base_uint_pday& a, uint32_t b) { return base_uint_pday(a) *= b; }
    friend inline bool operator==(const base_uint_pday& a, const base_uint_pday& b) { return memcmp(a.pn, b.pn, sizeof(a.pn)) == 0; }
    friend inline bool operator!=(const base_uint_pday& a, const base_uint_pday& b) { return memcmp(a.pn, b.pn, sizeof(a.pn)) != 0; }
    friend inline bool operator>(const base_uint_pday& a, const base_uint_pday& b) { return a.CompareTo(b) > 0; }
    friend inline bool operator<(const base_uint_pday& a, const base_uint_pday& b) { return a.CompareTo(b) < 0; }
    friend inline bool operator>=(const base_uint_pday& a, const base_uint_pday& b) { return a.CompareTo(b) >= 0; }
    friend inline bool operator<=(const base_uint_pday& a, const base_uint_pday& b) { return a.CompareTo(b) <= 0; }
    friend inline bool operator==(const base_uint_pday& a, uint64_t b) { return a.EqualTo(b); }
    friend inline bool operator!=(const base_uint_pday& a, uint64_t b) { return !a.EqualTo(b); }

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);
    std::string ToString() const;
    std::string ToStringReverseEndian() const;

    unsigned char* begin()
    {
        return (unsigned char*)&pn[0];
    }

    unsigned char* end()
    {
        return (unsigned char*)&pn[WIDTH];
    }

    const unsigned char* begin() const
    {
        return (unsigned char*)&pn[0];
    }

    const unsigned char* end() const
    {
        return (unsigned char*)&pn[WIDTH];
    }

    unsigned int size() const
    {
        return sizeof(pn);
    }

    uint64_t Get64(int n = 0) const
    {
        return pn[2 * n] | (uint64_t)pn[2 * n + 1] << 32;
    }

    uint32_t Get32(int n = 0) const
    {
        return pn[2 * n];
    }
    /**
     * Returns the position of the highest bit set plus one, or zero if the
     * value is zero.
     */
    unsigned int bits() const;

    uint64_t GetLow64() const
    {
        assert(WIDTH >= 2);
        return pn[0] | (uint64_t)pn[1] << 32;
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return sizeof(pn);
    }

    template <typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        s.write((char*)pn, sizeof(pn));
    }

    template <typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        s.read((char*)pn, sizeof(pn));
    }

    friend class uint160;
    friend class uint256;
    friend class uint512;
};

/** 512-bit unsigned big integer. */
class uint512 : public base_uint_pday<512>
{
public:
    uint512() {}
    uint512(const base_uint_pday<512>& b) : base_uint_pday<512>(b) {}
    uint512(uint64_t b) : base_uint_pday<512>(b) {}
    explicit uint512(const std::string& str) : base_uint_pday<512>(str) {}
    explicit uint512(const std::vector<unsigned char>& vch) : base_uint_pday<512>(vch) {}

    uint256 trim256() const
    {
        uint256 ret;
        for (unsigned int i = 0; i < uint256::WIDTH; i+=4) {
            ret.data[i] = (uint8_t)(pn[i] & 0xffff);
            ret.data[i + 1] = (uint8_t)((pn[i] >> 8) & 0xffff);
            ret.data[i + 2] =  (uint8_t)((pn[i] >> 16) & 0xffff);
            ret.data[i + 3] = (uint8_t)((pn[i] >> 24) & 0xffff);
        }
        return ret;
    }
};

inline uint512 uint512S(const std::string& str)
{
    uint512 rv;
    rv.SetHex(str);
    return rv;
}

#endif // PAYDAYCOIN_UINT256_H
