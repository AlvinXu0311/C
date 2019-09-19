#include "hw3.h"
#include "limits.h"

float_bits
float_i2f (int i)
{
  unsigned sign, exp, frac;	//represent the three fields
  unsigned leftmost, rightmost, t;	//auxiliary variable
  if (i == 0)
    return i;
  int shift;
  sign = ((i & INT_MIN) == INT_MIN);	//i>0 or i<0
  if (sign)
    i = ~i + 1;
  for (rightmost = 1, t = INT_MIN; (t & i) != t; t >>= 1)
    rightmost++;
  leftmost = (sizeof (int) << 3) - rightmost;
  exp = leftmost + 127;
  if (leftmost > 23)
    {
      shift = leftmost - 23;
      int mask = ((1 << shift) - 1) & i;
      int half = 1 << (shift - 1);
      int hide = 1 << shift;
      int round = mask > half || (mask == half && (i & hide) == hide);
      frac = (i >> shift) & 0x7FFFFF;
      if (frac == 0x7FFFFF && round == 1)
	{
	  frac = 0;
	  exp++;
	}
      else
	frac += round;
    }
  else
    {
      shift = 23 - leftmost;
      frac = (i << shift) & 0x7FFFFF;
    }
  return (sign << 31) | (exp << 23) | frac;
}
