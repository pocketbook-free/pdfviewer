//========================================================================
//
// GooString.h
//
// Simple variable-length string type.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef GooString_H
#define GooString_H

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <stdarg.h>
#include <stdlib.h> // for NULL
#include "gtypes.h"

class GooString {
public:

  // Create an empty string.
  GooString();

  // Create a string from a C string.
  GooString(const char *sA);

  // Create a string from <lengthA> chars at <sA>.  This string
  // can contain null characters.
  GooString(const char *sA, int lengthA);

  // Create a string from <lengthA> chars at <idx> in <str>.
  GooString(GooString *str, int idx, int lengthA);

  // Set content of a string to concatination of <s1> and <s2>. They can both
  // be NULL. if <s1Len> or <s2Len> is CALC_STRING_LEN, then length of the string
  // will be calculated with strlen(). Otherwise we assume they are a valid
  // length of string (or its substring)
  GooString* Set(const char *s1, int s1Len=CALC_STRING_LEN, const char *s2=NULL, int s2Len=CALC_STRING_LEN);

  // Copy a string.
  GooString(GooString *str);
  GooString *copy() { return new GooString(this); }

  // Concatenate two strings.
  GooString(GooString *str1, GooString *str2);

  // Convert an integer to a string.
  static GooString *fromInt(int x);

  // Create a formatted string.  Similar to printf, but without the
  // string overflow issues.  Formatting elements consist of:
  //     {<arg>:[<width>][.<precision>]<type>}
  // where:
  // - <arg> is the argument number (arg 0 is the first argument
  //   following the format string) -- NB: args must be first used in
  //   order; they can be reused in any order
  // - <width> is the field width -- negative to reverse the alignment;
  //   starting with a leading zero to zero-fill (for integers)
  // - <precision> is the number of digits to the right of the decimal
  //   point (for floating point numbers)
  // - <type> is one of:
  //     d, x, o, b -- int in decimal, hex, octal, binary
  //     ud, ux, uo, ub -- unsigned int
  //     ld, lx, lo, lb, uld, ulx, ulo, ulb -- long, unsigned long
  //     f, g -- double
  //     c -- char
  //     s -- string (char *)
  //     t -- GooString *
  //     w -- blank space; arg determines width
  // To get literal curly braces, use {{ or }}.
  static GooString *format(const char *fmt, ...);
  static GooString *formatv(const char *fmt, va_list argList);

  // Destructor.
  ~GooString();

  // Get length.
  int getLength() { return length; }

  // Get C string.
  char *getCString() { return s; }

  // Get <i>th character.
  char getChar(int i) { return s[i]; }

  // Change <i>th character.
  void setChar(int i, char c) { s[i] = c; }

  // Clear string to zero length.
  GooString *clear();

  // Append a character or string.
  GooString *append(char c);
  GooString *append(GooString *str);
  GooString *append(const char *str, int lengthA=CALC_STRING_LEN);

  // Append a formatted string.
  GooString *appendf(const char *fmt, ...);
  GooString *appendfv(const char *fmt, va_list argList);

  // Insert a character or string.
  GooString *insert(int i, char c);
  GooString *insert(int i, GooString *str);
  GooString *insert(int i, const char *str, int lengthA=CALC_STRING_LEN);

  // Delete a character or range of characters.
  GooString *del(int i, int n = 1);

  // Convert string to all-upper/all-lower case.
  GooString *upperCase();
  GooString *lowerCase();

  // Compare two strings:  -1:<  0:=  +1:>
  int cmp(GooString *str);
  int cmpN(GooString *str, int n);
  int cmp(const char *sA);
  int cmpN(const char *sA, int n);

  GBool hasUnicodeMarker(void);

private:
  // you can tweak this number for a different speed/memory usage tradeoffs.
  // In libc malloc() rounding is 16 so it's best to choose a value that
  // results in sizeof(GooString) be a multiple of 16.
  // 24 makes sizeof(GooString) to be 32.
  static const int STR_STATIC_SIZE = 24;
  // a special value telling that the length of the string is not given
  // so it must be calculated from the strings
  static const int CALC_STRING_LEN = -1;

  int  roundedSize(int len);

  char sStatic[STR_STATIC_SIZE];
  int length;
  char *s;

  void resize(int newLength);
  static void formatInt(long x, char *buf, int bufSize,
			GBool zeroFill, int width, int base,
			char **p, int *len);
  static void formatUInt(Gulong x, char *buf, int bufSize,
			 GBool zeroFill, int width, int base,
			 char **p, int *len);
  static void formatDouble(double x, char *buf, int bufSize, int prec,
			   GBool trim, char **p, int *len);
};

#endif
