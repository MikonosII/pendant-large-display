//******************************************************************************
//  @file Decimal32.h
//  @author Nicolai Shlapunov
//
//  @details DevCore: Packed Decimal Float class, header
//
// @section COPYRIGHT
//
//  Copyright (c) 2016-2026, Devtronic & Nicolai Shlapunov
//  All rights reserved.
//
// @section LICENSE
//
//  SPDX-License-Identifier: BSD-3-Clause
//
//  Software License Agreement (BSD 3-Clause License)
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//  3. Neither the name of Devtronic nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//
// @section SUPPORT
//
//  Devtronic invests time and resources providing this open source code,
//  please support Devtronic and open-source hardware/software by
//  donations and/or purchasing products from Devtronic.
//
// *****************************************************************************

#ifndef Decimal32_h
#define Decimal32_h

// *****************************************************************************
// ***   Includes   ************************************************************
// *****************************************************************************
#include <cstdint> // For int32_t/int64_t/uint32_t types and INT64_MAX

// *****************************************************************************
// ***   Decimal32   ***********************************************************
// *****************************************************************************
// * Decimal floating point number packed into a single uint32_t: 29 bits of
// * mantissa(two's complement) and 3 bits of exponent.
// *
// *   number = mantissa * 10^-exponent
// *
// * Exponent is the number of decimals, so unlike binary floating point it
// * can only make the number smaller, never bigger. Magnitude is limited by
// * the mantissa range in all cases, and every decimal takes one digit away
// * from the integer part:
// *
// *   exponent 0:   +/-268435455
// *   exponent 1:   +/-26843545.5
// *   exponent 3:   +/-268435.455
// *   exponent 7:   +/-26.8435455
// *
// * In exchange numbers like 0.1 are represented exactly, which binary
// * floating point can't do, and all arithmetic is integer arithmetic.
// *
// * Layout: bits 31..3 - mantissa, bits 2..0 - exponent. Mantissa in the high
// * bits makes its sign bit the sign bit of the word, so it is extracted by a
// * single arithmetic shift and stored without masking.
// *
// * IMPORTANT: the whole object is a single naturally aligned 32-bit word, so
// * it is read and written atomically by the MCU. That allows to share
// * numbers between tasks without a mutex: reader can't see a new mantissa
// * with an old exponent. Don't add any other data members or virtual functions to this
// * class - it will break the atomicity.
// *
// * Conventions:
// * - Exponent is stored instead of the scaler, so non power of ten scalers
// *   are unrepresentable by design. Constructor converts scaler to exponent
// *   and rounds it down to the nearest power of ten(scaler can come from
// *   external data like script parameters, so Break() can't be used there).
// * - Arithmetic with int32_t argument treats it as a WHOLE number:
// *   number + 5 adds 5.0, not 5 raw counts. Mantissa access via GetMantissa().
// * - All results are clamped(saturated) to the mantissa range instead of
// *   wrapping. Division by zero saturates by dividend sign.
// * - Addition/subtraction/comparison align to the greater exponent, which is
// *   lossless: comparisons are exact.
// * - Division increases the exponent when the result needs more decimals
// *   than the dividend has: 1 / 2 gives 0.5, not 0. Exact results keep the
// *   resolution of the dividend: 10.500 / 2 gives 5.250.
// *****************************************************************************
class Decimal32
{
  public:
    // Number of bits used for the exponent
    static constexpr uint32_t EXP_BITS = 3u;
    // Maximum exponent value
    static constexpr uint8_t EXP_MAX = 7u;
    // Sign bit of the packed word
    static constexpr uint32_t SIGN_MASK = 0x80000000u;
    // Bits that have to be set to sign-extend a negative mantissa after the
    // exponent bits are shifted out
    static constexpr uint32_t SIGN_EXT_MASK = ~(0xFFFFFFFFu >> EXP_BITS);
    // Maximum mantissa: 2^28 - 1
    static constexpr int32_t MANTISSA_MAX = 268435455;
    // Minimum mantissa: -2^28
    static constexpr int32_t MANTISSA_MIN = -268435456;
    // Maximum precision for ToString()
    static constexpr uint8_t PRECISION_MAX = EXP_MAX;
    // Buffer size enough for any value with any precision: sign, 9 digits of
    // the whole part, decimal point, 7 digits of the fractional part and
    // null-terminator.
    static constexpr uint32_t STR_SIZE = 19u;

    // Precision types for ToString()
    // Exactly the requested number of decimals: the number is padded with
    // zeros or cut(not rounded) to fit
    static constexpr bool PRECISION_HARD = false;
    // At least the requested number of decimals, but all significant ones are
    // printed even if there are more than requested
    static constexpr bool PRECISION_SOFT = true;

    // Alignment for ToString()
    // Number is placed at the beginning of the buffer
    static constexpr bool ALIGN_LEFT = false;
    // Number is padded with spaces on the left to fill the whole buffer, so
    // the last digit is always at the same position and numbers of different
    // length line up in a column
    static constexpr bool ALIGN_RIGHT = true;

    // *************************************************************************
    // ***   Decimal32   *******************************************************
    // *************************************************************************
    Decimal32() {}

    // *************************************************************************
    // ***   Decimal32   *******************************************************
    // *************************************************************************
    // Explicit to prevent unintended implicit conversions from plain integers
    explicit Decimal32(int32_t val)
    {
      Pack(val, 0u);
    }

    // *************************************************************************
    // ***   Decimal32   *******************************************************
    // *************************************************************************
    Decimal32(int32_t val, uint32_t scal)
    {
      Pack(val, ScalerToExp(scal));
    }

    // *************************************************************************
    // ***   GetMantissa   ********************************************************
    // *************************************************************************
    inline int32_t GetMantissa() const
    {
      // Right shift of a signed value is implementation defined before C++20,
      // so the sign bits are restored explicitly. Costs nothing: compiler
      // that shifts arithmetically optimizes the OR out.
      return ((int32_t)packed >> EXP_BITS) | ((packed & SIGN_MASK) ? SIGN_EXT_MASK : 0u);
    }

    // *************************************************************************
    // ***   GetExp   **********************************************************
    // *************************************************************************
    inline uint8_t GetExp() const
    {
      return (uint8_t)(packed & EXP_MAX);
    }

    // *************************************************************************
    // ***   GetScaler   *******************************************************
    // *************************************************************************
    // Result is uint32_t: exponent 5 and above doesn't fit into uint16_t
    inline uint32_t GetScaler() const
    {
      return Pow10(GetExp());
    }

    // *************************************************************************
    // ***   GetSignificantDecimals   ******************************************
    // *************************************************************************
    // * Number of decimals left after trailing zeros are stripped: 1.500 with
    // * exponent 3 has one significant decimal.
    uint8_t GetSignificantDecimals() const
    {
      uint8_t exp = GetExp();
      // Fractional part of the value
      uint32_t frac = Magnitude() % GetScaler();
      // Strip trailing zeros
      while((exp > 0u) && ((frac % 10u) == 0u))
      {
        frac /= 10u;
        exp--;
      }
      return exp;
    }

    // *************************************************************************
    // ***   GetPacked   *******************************************************
    // *************************************************************************
    // Raw packed representation - for storing values in NVM or transferring
    // them between tasks
    inline uint32_t GetPacked() const
    {
      return packed;
    }

    // *************************************************************************
    // ***   SetPacked   *******************************************************
    // *************************************************************************
    // Restore value from the raw packed representation
    inline void SetPacked(uint32_t val)
    {
      packed = val;
    }

    // *************************************************************************
    // ***   ToString   ********************************************************
    // *************************************************************************
    // * Print value into the provided buffer using the stored exponent as the
    // * precision. Returns buf, so the call can be used as an argument of
    // * snprintf() directly.
    inline char* ToString(char* buf, uint32_t buf_size) const
    {
      return ToString(buf, buf_size, GetExp(), PRECISION_HARD, ALIGN_LEFT);
    }

    // *************************************************************************
    // ***   ToString   ********************************************************
    // *************************************************************************
    // * Print value into the provided buffer. Type qualifies the precision,
    // * so those two stay together and the alignment - which is only needed
    // * for a fixed width field - goes last and has a default.
    // *
    // * Buffer should be at least STR_SIZE bytes long, otherwise the result
    // * is truncated(and still null-terminated). With ALIGN_RIGHT the number
    // * is padded with spaces to fill the whole buffer, so the buffer size
    // * sets the field width. Digits are generated
    // * directly instead of using snprintf(): it saves stack, code space and
    // * keeps this class free of any dependencies. Returns buf, so the call
    // * can be used as an argument of snprintf() directly.
    char* ToString(char* buf, uint32_t buf_size, uint8_t precision, bool type = PRECISION_HARD, bool align = ALIGN_LEFT) const
    {
      // Check buffer before use
      if((buf != nullptr) && (buf_size > 0u))
      {
        // Limit precision to the maximum supported
        if(precision > PRECISION_MAX) precision = PRECISION_MAX;

        // Number of decimals to print
        uint8_t out_prec = precision;
        // For soft precision all significant decimals have to be printed
        if(type == PRECISION_SOFT)
        {
          uint8_t significant = GetSignificantDecimals();
          if(significant > out_prec) out_prec = significant;
        }
        else
        {
          ; // Do nothing - MISRA rule
        }

        // Sign have to be handled separately: it would be lost for numbers
        // whose whole part is zero. Negation is safe - MANTISSA_MIN fits into int32_t
        // negated, unlike INT32_MIN.
        bool negative = (GetMantissa() < 0);
        uint32_t mag = Magnitude();
        uint8_t exp = GetExp();
        // Split value into the whole and the fractional parts
        uint32_t whole = mag / GetScaler();
        uint32_t frac = mag % GetScaler();
        // Scale fractional part from the stored exponent to the requested
        // precision: pad with zeros or cut(not round) extra decimals. Result
        // is always less than 10^out_prec, so it can't overflow.
        if(out_prec >= exp) frac *= Pow10(out_prec - exp);
        else                frac /= Pow10(exp - out_prec);

        // Digits are generated in reverse order into the local buffer. Worst
        // case is sign, 9 digits of the whole part, point and 7 decimals -
        // exactly STR_SIZE - 1 characters.
        char tmp[STR_SIZE];
        uint32_t idx = 0u;

        // Fractional part digits and the decimal point
        if(out_prec > 0u)
        {
          for(uint8_t i = 0u; i < out_prec; i++)
          {
            tmp[idx++] = (char)('0' + (frac % 10u));
            frac /= 10u;
          }
          tmp[idx++] = '.';
        }
        else
        {
          ; // Do nothing - MISRA rule
        }

        // Whole part digits - at least one digit is always printed
        do
        {
          tmp[idx++] = (char)('0' + (whole % 10u));
          whole /= 10u;
        } while(whole != 0u);

        // Sign
        if(negative) tmp[idx++] = '-';

        // Number of characters written into the caller buffer
        uint32_t n = 0u;

        // For right alignment fill the beginning of the buffer with spaces,
        // so the number ends at the last position. Nothing is added if it
        // doesn't fit - the number itself is never sacrificed for padding.
        if(align == ALIGN_RIGHT)
        {
          while((n + idx) < (buf_size - 1u))
          {
            buf[n] = ' ';
            n++;
          }
        }

        // Copy digits into the caller buffer in the direct order. One byte is
        // always reserved for the null-terminator, so a small buffer gets a
        // truncated, but valid, string.
        while((idx > 0u) && (n < (buf_size - 1u)))
        {
          idx--;
          buf[n] = tmp[idx];
          n++;
        }
        buf[n] = '\0';
      }
      else
      {
        ; // Do nothing - MISRA rule
      }

      // Return buffer for the caller convenience
      return buf;
    }

    // *************************************************************************
    // ***   FromString   ******************************************************
    // *************************************************************************
    // * Set value from the string. Exponent is taken from the number of
    // * decimals in the string and limited to EXP_MAX - extra decimals are
    // * cut(not rounded). Value out of range is clamped. Object isn't
    // * modified if the string doesn't contain a number.
    // *
    // * Returns the number of characters the number took, everything
    // * included: leading spaces, sign, digits and decimal point. So the
    // * result is zero(false) if there is no number, and str + result points
    // * to the first character after the number otherwise - a caller parsing
    // * a list can check the separator and go on from there.
    uint32_t FromString(const char* str)
    {
      uint32_t result = 0u;

      // Check pointer before use
      if(str != nullptr)
      {
        const char* p = str;
        // Skip leading spaces
        while(*p == ' ') p++;

        // Sign
        bool negative = false;
        if((*p == '-') || (*p == '+'))
        {
          negative = (*p == '-');
          p++;
        }

        // Wider type to detect out of range mantissa
        int64_t val = 0;
        // Exponent - number of decimals found in the string
        uint8_t exp = 0u;
        // Flags
        bool point_found = false;
        bool digit_found = false;

        // Parse the number
        while(*p != '\0')
        {
          if((*p >= '0') && (*p <= '9'))
          {
            digit_found = true;
            // Fractional digits beyond the maximum exponent are cut, and
            // accumulation stops as soon as the mantissa is out of range - it
            // will be clamped anyway, and int64_t can't overflow that way.
            if(!(point_found && (exp >= EXP_MAX)) && (val <= MANTISSA_MAX))
            {
              val = (val * 10) + (int64_t)(*p - '0');
              if(point_found) exp++;
            }
            else
            {
              ; // Do nothing - MISRA rule
            }
          }
          else if((*p == '.') && !point_found)
          {
            point_found = true;
          }
          else
          {
            // Any other character ends the number
            break;
          }
          p++;
        }

        // Set value only if at least one digit was found
        if(digit_found)
        {
          if(negative) val = -val;
          Pack(val, exp);
          // Parsing stopped at the first character that isn't part of the number
          result = (uint32_t)(p - str);
        }
        else
        {
          ; // Do nothing - MISRA rule
        }
      }

      // Return result
      return result;
    }

    // *************************************************************************
    // ***   ToFixedPoint   ****************************************************
    // *************************************************************************
    // * Value as a whole number of 1/scaler units - the opposite of the
    // * constructor with scaler: 12.700 with scaler 1000 gives 12700.
    // *
    // * Result is exact if the number has no more decimals than the scaler.
    // * Extra decimals are cut(not rounded), the same way ToString() and
    // * FromString() do it: 1.2345 with scaler 1000 gives 1234. Result out of
    // * int32_t range is clamped.
    int32_t ToFixedPoint(uint32_t scal) const
    {
      // Scaler of the number itself
      uint32_t own_scal = GetScaler();
      // If scalers are the same(the usual case) mantissa is the result
      int64_t val = GetMantissa();
      // Otherwise it has to be rescaled. Mantissa has 29 bits, so the
      // product with any 32-bit scaler fits into int64_t.
      if(own_scal != scal)
      {
        val = (val * (int64_t)scal) / (int64_t)own_scal;
        // Clamp to the result range
        if(val > INT32_MAX) val = INT32_MAX;
        if(val < INT32_MIN) val = INT32_MIN;
      }
      // Return result
      return (int32_t)val;
    }

    // *************************************************************************
    // ***   operator=   *******************************************************
    // *************************************************************************
    // * Set value from a binary floating point number.
    // *
    // * A binary float can't hold most decimal fractions, so the value has to
    // * be recovered rather than converted: 0.3 is stored in a double as
    // * 0.29999999999999998890 and a plain conversion would give 0.2999999.
    // *
    // * Values that don't fit are clamped, NaN gives zero. Costs nothing if
    // * unused: the operator is inline, so floating point code isn't linked
    // * unless it is called.
    Decimal32& operator=(double val)
    {
      AssignFloatingPoint(val, false);
      return *this;
    }

    // *************************************************************************
    // ***   operator=   *******************************************************
    // *************************************************************************
    // * Set value from a single precision number - see operator=(double). The
    // * source type is passed along: a float keeps far fewer digits, and the
    // * recovery has to know that to not mistake its error for the value.
    Decimal32& operator=(float val)
    {
      AssignFloatingPoint((double)val, true);
      return *this;
    }

    // *************************************************************************
    // ***   operator float   **************************************************
    // *************************************************************************
    // * Explicit on purpose: implicit conversion would let the number slip
    // * into floating point expressions unnoticed, which defeats the exact
    // * decimal arithmetic this class exists for. Use (float)value to ask
    // * for it.
    // *
    // * Note that float has 24 bits of mantissa while this class has 29, so
    // * values above ~16.7 million are rounded: 268435.455 converts to
    // * 268435.468750.
    explicit operator float() const
    {
      return ((float)GetMantissa() / (float)GetScaler());
    }

    // *************************************************************************
    // ***   operator double   *************************************************
    // *************************************************************************
    // * Explicit for the same reason as operator float().
    // *
    // * Conversion is NOT exact: a binary floating point number can't
    // * represent most decimal fractions at any precision(0.1 becomes
    // * 0.10000000000000000555), which is the reason this class exists.
    // * Double only has enough mantissa bits to keep all 29 bits of the
    // * value, so it is the closest possible approximation - use it when the
    // * number has to be handed to floating point math, not to store it.
    // *
    // * Double isn't supported by the single precision FPU either, so the
    // * conversion and everything done with the result is emulated in
    // * software. Costs nothing if unused: the operator is inline and the
    // * soft float routines aren't linked unless it is called.
    explicit operator double() const
    {
      return ((double)GetMantissa() / (double)GetScaler());
    }

    // *************************************************************************
    // ***   operator- (unary)   ***********************************************
    // *************************************************************************
    Decimal32 operator-() const
    {
      Decimal32 result;
      // Clamp handles MANTISSA_MIN which can't be negated in the mantissa range
      result.Pack(-(int64_t)GetMantissa(), GetExp());
      return result;
    }

    // *************************************************************************
    // ***   operator+   *******************************************************
    // *************************************************************************
    Decimal32 operator+(int32_t r_arg) const
    {
      Decimal32 result = *this;
      result += r_arg;
      return result;
    }

    // *************************************************************************
    // ***   operator+=   ******************************************************
    // *************************************************************************
    Decimal32& operator+=(int32_t r_arg)
    {
      // Argument is a whole number - scale it before addition
      Pack((int64_t)GetMantissa() + (int64_t)r_arg * GetScaler(), GetExp());
      return *this;
    }

    // *************************************************************************
    // ***   operator-   *******************************************************
    // *************************************************************************
    Decimal32 operator-(int32_t r_arg) const
    {
      Decimal32 result = *this;
      result -= r_arg;
      return result;
    }

    // *************************************************************************
    // ***   operator-=   ******************************************************
    // *************************************************************************
    Decimal32& operator-=(int32_t r_arg)
    {
      // Argument is a whole number - scale it before subtraction
      Pack((int64_t)GetMantissa() - (int64_t)r_arg * GetScaler(), GetExp());
      return *this;
    }

    // *************************************************************************
    // ***   operator*   *******************************************************
    // *************************************************************************
    Decimal32 operator*(int32_t r_arg) const
    {
      Decimal32 result = *this;
      result *= r_arg;
      return result;
    }

    // *************************************************************************
    // ***   operator*=   ******************************************************
    // *************************************************************************
    Decimal32& operator*=(int32_t r_arg)
    {
      // Multiplication by a whole number: exponent stays the same
      Pack((int64_t)GetMantissa() * r_arg, GetExp());
      return *this;
    }

    // *************************************************************************
    // ***   operator/   *******************************************************
    // *************************************************************************
    Decimal32 operator/(int32_t r_arg) const
    {
      Decimal32 result = *this;
      result /= r_arg;
      return result;
    }

    // *************************************************************************
    // ***   operator/=   ******************************************************
    // *************************************************************************
    Decimal32& operator/=(int32_t r_arg)
    {
      // Division by zero: saturate by dividend sign instead of fault
      if(r_arg == 0)
      {
        Pack((GetMantissa() < 0) ? MANTISSA_MIN : MANTISSA_MAX, GetExp());
      }
      else
      {
        // Division by a whole number: exponent of the result is the same or
        // greater, PackQuotient() scales the dividend to keep the decimals
        PackQuotient(GetMantissa(), GetExp(), r_arg);
      }
      return *this;
    }

    // *************************************************************************
    // ***   operator+   *******************************************************
    // *************************************************************************
    Decimal32 operator+(const Decimal32& r_arg) const
    {
      Decimal32 result = *this;
      result += r_arg;
      return result;
    }

    // *************************************************************************
    // ***   operator+=   ******************************************************
    // *************************************************************************
    Decimal32& operator+=(const Decimal32& r_arg)
    {
      // Exponents of both values
      uint8_t exp = GetExp();
      uint8_t r_exp = r_arg.GetExp();
      // Wider type to prevent overflow during exponent alignment and addition
      int64_t val = GetMantissa();

      if(exp < r_exp)
      {
        // Align current mantissa to the greater exponent
        val *= Pow10(r_exp - exp);
        // Add value
        val += r_arg.GetMantissa();
        // Update exponent
        exp = r_exp;
      }
      else if(exp > r_exp)
      {
        // Align added mantissa to the greater exponent and add it
        val += (int64_t)r_arg.GetMantissa() * Pow10(exp - r_exp);
      }
      else
      {
        // Exponents are the same, just add value
        val += r_arg.GetMantissa();
      }

      // Normalize, clamp and store result
      Pack(val, exp);

      return *this;
    }

    // *************************************************************************
    // ***   operator-   *******************************************************
    // *************************************************************************
    Decimal32 operator-(const Decimal32& r_arg) const
    {
      Decimal32 result = *this;
      result -= r_arg;
      return result;
    }

    // *************************************************************************
    // ***   operator-=   ******************************************************
    // *************************************************************************
    Decimal32& operator-=(const Decimal32& r_arg)
    {
      // Exponents of both values
      uint8_t exp = GetExp();
      uint8_t r_exp = r_arg.GetExp();
      // Wider type to prevent overflow during exponent alignment and subtraction
      int64_t val = GetMantissa();

      if(exp < r_exp)
      {
        // Align current mantissa to the greater exponent
        val *= Pow10(r_exp - exp);
        // Subtract value
        val -= r_arg.GetMantissa();
        // Update exponent
        exp = r_exp;
      }
      else if(exp > r_exp)
      {
        // Align subtracted mantissa to the greater exponent and subtract it
        val -= (int64_t)r_arg.GetMantissa() * Pow10(exp - r_exp);
      }
      else
      {
        // Exponents are the same, just subtract value
        val -= r_arg.GetMantissa();
      }

      // Normalize, clamp and store result
      Pack(val, exp);

      return *this;
    }

    // *************************************************************************
    // ***   operator*   *******************************************************
    // *************************************************************************
    Decimal32 operator*(const Decimal32& r_arg) const
    {
      Decimal32 result = *this;
      result *= r_arg;
      return result;
    }

    // *************************************************************************
    // ***   operator*=   ******************************************************
    // *************************************************************************
    Decimal32& operator*=(const Decimal32& r_arg)
    {
      // Exponents of both values
      uint8_t exp = GetExp();
      uint8_t r_exp = r_arg.GetExp();
      // Result exponent is the greater one, and the product have to be
      // divided by the lesser one to bring it back
      uint8_t max_exp = (exp > r_exp) ? exp : r_exp;
      uint8_t min_exp = (exp < r_exp) ? exp : r_exp;
      // Wider type to prevent overflow
      int64_t val = (int64_t)GetMantissa() * r_arg.GetMantissa();
      // Bring value back after multiplication
      val /= Pow10(min_exp);
      // Normalize, clamp and store result
      Pack(val, max_exp);

      return *this;
    }

    // *************************************************************************
    // ***   operator/   *******************************************************
    // *************************************************************************
    Decimal32 operator/(const Decimal32& r_arg) const
    {
      Decimal32 result = *this;
      result /= r_arg;
      return result;
    }

    // *************************************************************************
    // ***   operator/=   ******************************************************
    // *************************************************************************
    Decimal32& operator/=(const Decimal32& r_arg)
    {
      // Division by zero: saturate by dividend sign instead of fault
      if(r_arg.GetMantissa() == 0)
      {
        Pack((GetMantissa() < 0) ? MANTISSA_MIN : MANTISSA_MAX, GetExp());
      }
      else
      {
        // Wider type to prevent overflow. Dividend is scaled by the divisor
        // scaler to bring the result back to the current exponent.
        int64_t val = (int64_t)GetMantissa() * Pow10(r_arg.GetExp());
        // Divide, scaling the dividend to keep as many decimals as possible
        PackQuotient(val, GetExp(), r_arg.GetMantissa());
      }

      return *this;
    }

    // *************************************************************************
    // ***   operator==   ******************************************************
    // *************************************************************************
    inline bool operator==(const Decimal32& r_arg) const
    {
      return(GetAlignedMantissa(*this, r_arg) == GetAlignedMantissa(r_arg, *this));
    }

    // *************************************************************************
    // ***   operator!=   ******************************************************
    // *************************************************************************
    inline bool operator!=(const Decimal32& r_arg) const
    {
      return(!(*this == r_arg));
    }

    // *************************************************************************
    // ***   operator>   *******************************************************
    // *************************************************************************
    inline bool operator>(const Decimal32& r_arg) const
    {
      return(GetAlignedMantissa(*this, r_arg) > GetAlignedMantissa(r_arg, *this));
    }

    // *************************************************************************
    // ***   operator>=   ******************************************************
    // *************************************************************************
    inline bool operator>=(const Decimal32& r_arg) const
    {
      return(GetAlignedMantissa(*this, r_arg) >= GetAlignedMantissa(r_arg, *this));
    }

    // *************************************************************************
    // ***   operator<   *******************************************************
    // *************************************************************************
    inline bool operator<(const Decimal32& r_arg) const
    {
      return(GetAlignedMantissa(*this, r_arg) < GetAlignedMantissa(r_arg, *this));
    }

    // *************************************************************************
    // ***   operator<=   ******************************************************
    // *************************************************************************
    inline bool operator<=(const Decimal32& r_arg) const
    {
      return(GetAlignedMantissa(*this, r_arg) <= GetAlignedMantissa(r_arg, *this));
    }

  private:
    // Packed value: bits 31..3 - value(two's complement), bits 2..0 - exponent
    uint32_t packed = 0u;

    // *************************************************************************
    // ***   Pack   ************************************************************
    // *************************************************************************
    // * Store mantissa and exponent - the only place where the packed word is
    // * created.
    // *
    // * Exponent isn't reduced for numbers that fit: it is the resolution of
    // * the number, not just a scale factor, and neither arithmetic nor the
    // * constructor should silently make it coarser. 10.500 + 0.000 stays
    // * 10.500, not 10.5.
    // *
    // * Operations use the greater exponent of the operands, so the mantissa
    // * can be out of range even when the number is small: 100.000 multiplied
    // * by 1.0000000 needs 10^9 at exponent 7. In that case decimals are
    // * dropped until the value fits - losing precision is much better than
    // * losing the magnitude, which is what plain clamping would do.
    void Pack(int64_t val, uint8_t exp)
    {
      // Drop decimals while the mantissa is out of range
      while((exp > 0u) && ((val > MANTISSA_MAX) || (val < MANTISSA_MIN)))
      {
        val /= 10;
        exp--;
      }
      // Saturate: at exponent 0 there is nothing left to drop, and an out of
      // range mantissa would overwrite the exponent bits
      if(val > MANTISSA_MAX) val = MANTISSA_MAX;
      if(val < MANTISSA_MIN) val = MANTISSA_MIN;
      // Store mantissa and exponent
      packed = (((uint32_t)(int32_t)val) << EXP_BITS) | (exp & EXP_MAX);
    }

    // *************************************************************************
    // ***   PackQuotient   ****************************************************
    // *************************************************************************
    // * Store the result of a division.
    // *
    // * Dividing without scaling would truncate the whole fractional part of
    // * the result: mantissa 1 with exponent 0 divided by 2 would give 0
    // * instead of 0.5. So the dividend is scaled up first to get as many
    // * decimals as possible, and the trailing zeros are stripped after the
    // * division to keep the original resolution when the result allows it:
    // * 10.500 / 2 gives 5.250, not 5.2500000.
    // *
    // * Scaling is limited by int64_t as well: the dividend can already be as
    // * big as mantissa * 10^7 for the Decimal32 divisor, and scaling it by
    // * another 10^7 would overflow.
    void PackQuotient(int64_t val, uint8_t exp, int64_t divisor)
    {
      // Exponent of the dividend - result is not reduced below it
      uint8_t min_exp = exp;

      // Scale dividend up while the exponent allows and it fits into int64_t
      while((exp < EXP_MAX) && (val > -(INT64_MAX / 10)) && (val < (INT64_MAX / 10)))
      {
        val *= 10;
        exp++;
      }

      // Divide values
      val /= divisor;

      // Strip trailing zeros added by the scaling above, but don't make the
      // result coarser than the dividend was
      while((exp > min_exp) && ((val % 10) == 0))
      {
        val /= 10;
        exp--;
      }

      // Normalize, clamp and store result
      Pack(val, exp);
    }

    // *************************************************************************
    // ***   AssignFloatingPoint   *********************************************
    // *************************************************************************
    // * Recover a decimal number from its binary floating point value.
    // *
    // * Tries every exponent starting from zero and takes the first one that
    // * converts back to exactly the same floating point number: that is the
    // * shortest decimal form the source value can represent, which is the
    // * number that was meant. 0.3 stored as 0.29999999999999998890 gives
    // * mantissa 3 with exponent 1, not 2999999 with exponent 7.
    // *
    // * Comparison have to be done in the source type: a float keeps about 7
    // * significant digits, so 25.4f is 25.399999618530273 and matches 25.4
    // * only when both sides are rounded back to float.
    // *
    // * If nothing converts back exactly(a number with more decimals than can
    // * be stored, like 1/3) the most precise representation that fits is
    // * used, with trailing zeros stripped.
    void AssignFloatingPoint(double val, bool single_precision)
    {
      // NaN can't be represented in any way - set value to zero
      if(val != val)
      {
        Pack(0, 0u);
      }
      else
      {
        // Best representation found so far
        int64_t best_mantissa = 0;
        uint8_t best_exp = 0u;
        // Flags: at least one exponent fits, and the value converts back
        bool fits = false;
        bool exact = false;

        for(uint8_t exp = 0u; exp <= EXP_MAX; exp++)
        {
          // Scale value by the exponent
          double scaled = val * (double)Pow10(exp);
          // Stop if it doesn't fit even after rounding - a greater exponent
          // can't fit either. Also catches infinity, which must not be
          // converted to an integer type.
          if((scaled > ((double)MANTISSA_MAX + 0.5)) ||
             (scaled < ((double)MANTISSA_MIN - 0.5))) break;

          // Round to the nearest integer instead of truncating
          int64_t mantissa = (int64_t)((scaled < 0.0) ? (scaled - 0.5) : (scaled + 0.5));
          // Remember it - the last one that fits is the most precise
          best_mantissa = mantissa;
          best_exp = exp;
          fits = true;

          // Check if it converts back to the same number
          double back = (double)mantissa / (double)Pow10(exp);
          if(single_precision ? ((float)back == (float)val) : (back == val))
          {
            exact = true;
            break;
          }
        }

        if(!fits)
        {
          // Value is out of range even without decimals - clamp it
          Pack((val < 0.0) ? MANTISSA_MIN : MANTISSA_MAX, 0u);
        }
        else
        {
          // Nothing converted back exactly, so the most precise form is used
          // and it can hold trailing zeros - strip them for the shortest form
          if(!exact)
          {
            while((best_exp > 0u) && ((best_mantissa % 10) == 0))
            {
              best_mantissa /= 10;
              best_exp--;
            }
          }
          // Store result
          Pack(best_mantissa, best_exp);
        }
      }
    }

    // *************************************************************************
    // ***   Magnitude   *******************************************************
    // *************************************************************************
    // * Absolute value of the mantissa. Negation is safe: MANTISSA_MIN negated
    // * fits into int32_t, unlike INT32_MIN.
    uint32_t Magnitude() const
    {
      int32_t val = GetMantissa();
      return (uint32_t)((val < 0) ? -val : val);
    }

    // *************************************************************************
    // ***   Pow10   ***********************************************************
    // *************************************************************************
    // * Power of ten for the given exponent. Index is masked, so out of range
    // * exponent can't cause out of bound access.
    static uint32_t Pow10(uint8_t exp)
    {
      static const uint32_t pow10[EXP_MAX + 1u] = {1u, 10u, 100u, 1000u, 10000u,
                                                   100000u, 1000000u, 10000000u};
      return pow10[exp & EXP_MAX];
    }

    // *************************************************************************
    // ***   ScalerToExp   *****************************************************
    // *************************************************************************
    // * Convert scaler to exponent. Scaler have to be a power of ten, any
    // * other value is rounded down to the nearest power of ten.
    static uint8_t ScalerToExp(uint32_t scal)
    {
      uint8_t exp = 0u;
      uint32_t s = 1u;
      // Increase exponent until the next power of ten is greater than scaler
      while((exp < EXP_MAX) && (scal >= (s * 10u)))
      {
        s *= 10u;
        exp++;
      }
      return exp;
    }

    // *************************************************************************
    // ***   GetAlignedMantissa   *************************************************
    // *************************************************************************
    // * Returns mantissa of val1 aligned to the greater of two exponents.
    // * Alignment to the greater exponent is lossless, and int64_t result
    // * can't overflow, so comparisons based on it are exact.
    static int64_t GetAlignedMantissa(const Decimal32& val1, const Decimal32& val2)
    {
      // Save first value
      int64_t val = val1.GetMantissa();
      // If first value exponent is less - align it
      if(val1.GetExp() < val2.GetExp())
      {
        val *= Pow10(val2.GetExp() - val1.GetExp());
      }
      // Return result
      return val;
    }
};

#endif
