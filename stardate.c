/*
 *  stardate: convert between date formats
 *  by Andrew Main <zefram@fysh.org>
 *  1997-12-26, stardate [-30]0458.96
 *
 *  Stardate code is based on version 1 of the Stardates in Star Trek FAQ.
 */

/*
 * Copyright (c) 1996, 1997 Andrew Main.  All rights reserved.
 *
 * Redistribution and use, in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *        This product includes software developed by Andrew Main.
 * 4. The name of Andrew Main may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANDREW MAIN BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Unix programmers, please excuse the occasional DOSism in this code.
 *  DOS programmers, please excuse the Unixisms.  All programmers, please
 *  excuse the ANSIisms.  This program should actually run anywhere; I've
 *  tried to make it strictly conforming C.
 */

/*
 *  This program converts between dates in five formats:
 *    - stardates
 *    - the Julian calendar (with UTC time)
 *    - the Gregorian calendar (with UTC time)
 *    - the Quadcent calendar (see the Stardates FAQ for explanation)
 *    - traditional Unix time (seconds since 1970-01-01T00:00Z)
 *  Input and output can be in any of these formats.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* compensate for ANSI-challenged <stdlib.h> */

#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

/* for convenience (isxxx() want an unsigned char input) */

#define ISDIGIT(c) isdigit((unsigned char)(c))
#define ISALNUM(c) isalnum((unsigned char)(c))

/* atleast types */

typedef unsigned char uint1;

typedef unsigned int uint16;

typedef unsigned long uint32;
#define UINT32_HIGH 0xffffffffUL

typedef struct {
  uint32 high; /* multiples of 2^32 */
  uint32 low; /* range 0-(2^32-1) */
} uint64;
#define UINT64_DIGITS 20

/* To avoid using the raw type name avoidably */

typedef int errnot;

/* Internal date format: an extended Unix-style date format.
 * This consists of the number of seconds since 0001=01=01 stored in a
 * 64 bit type, plus an additional 32 bit fraction of a second.
 * Each of the date formats used for I/O has a pair of functions,
 * used for converting from/to the internal format.
 */

typedef struct {
  uint64 sec; /* seconds since 0001=01=01; unlimited range */
  uint32 frac; /* range 0-(2^32-1) */
} intdate;

static void getcurdate(intdate *);
static void output(intdate const *);

static uint16 sdin(char const *, intdate *);
static uint16 julin(char const *, intdate *);
static uint16 gregin(char const *, intdate *);
static uint16 qcin(char const *, intdate *);
static uint16 unixin(char const *, intdate *);

static char const *sdout(intdate const *);
static char const *julout(intdate const *);
static char const *gregout(intdate const *);
static char const *qcout(intdate const *);
static char const *unixdout(intdate const *);
static char const *unixxout(intdate const *);

static struct format {
  char opt;
  uint1 sel;
  /* The `in' function takes a string, and interprets it.  It returns 0     *
   * if the string is unrecognisable, 1 if it was successfully interpreted, *
   * or 2 if the string was recognised but invalid.  The date is returned   *
   * in the intdate passed to the function.  The function should round the  *
   * fraction *up*, so that on output the downward rounding won't cause     *
   * wildly inaccurate changes of date.  This makes it theoretically        *
   * possible for a date output in a different format from the input to be  *
   * up to 1/2^32 second too late, but this is much less serious.           */
  uint16 (*in)(char const *, intdate *);
  /* The `out' function takes an internal date, and converts it to a string *
   * for output.  The string may be in static memory.                       */
  char const *(*out)(intdate const *);
} formats[] = {
  { 's', 0, sdin,   sdout    },
  { 'j', 0, julin,  julout   },
  { 'g', 0, gregin, gregout  },
  { 'q', 0, qcin,   qcout    },
  { 'u', 0, unixin, unixdout },
  { 'x', 0, NULL,   unixxout },
  { 0, 0, NULL, NULL }
};

static uint16 sddigits = 2;

static char const *progname;

int main(int argc, char **argv)
{
  struct format *f;
  uint1 sel = 0, haderr = 0;
  char *ptr;
  intdate dt;
  if((ptr = strrchr(*argv, '/')) || (ptr = strrchr(*argv, '\\')))
    progname = ptr+1;
  else
    progname = *argv;
  if(!*progname)
    progname = "stardate";
  while(*++argv && **argv == '-')
    while(*++*argv) {
      for(f = formats; f->opt; f++)
	if(**argv == f->opt) {
	  f->sel = sel = 1;
	  goto got;
	}
      fprintf(stderr, "%s: bad option: -%c\n", progname, **argv);
      exit(EXIT_FAILURE);
      got:
      if(**argv == 's' && argv[0][1] >= '0' && argv[0][1] <= '6')
	sddigits = *++*argv - '0';
    }
  if(!sel)
    formats[0].sel = 1;
  if(!*argv) {
    getcurdate(&dt);
    output(&dt);
  } else {
    do {
      uint16 n = 0;
      for(f = formats; f->opt; f++) {
	errno = 0;
	if(f->in && (n = f->in(*argv, &dt)))
	  break;
      }
      haderr |= !(n & 1);
      if(!n)
	fprintf(stderr, "%s: date format unrecognised: %s\n", progname, *argv);
      else if(n == 1) {
	if(errno) {
	  fprintf(stderr, "%s: date is out of acceptable range: %s\n",
	      progname, *argv);
	  haderr = 1;
	} else
	  output(&dt);
      }
    } while(*++argv);
  }
  exit(haderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void getcurdate(intdate *dt)
{
  time_t t = time(NULL);
  struct tm *tm = gmtime(&t);
  char utc[20];
  sprintf(utc, "%04d-%02d-%02dT%02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon+1,
      tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
  gregin(utc, dt);
}

static void output(intdate const *dt)
{
  struct format *f;
  uint1 d1 = 0;
  for(f = formats; f->opt; f++)
    if(f->sel) {
      if(d1)
	putchar(' ');
      d1 = 1;
      fputs(f->out(dt), stdout);
    }
  putchar('\n');
}

#define UINT64INIT(high, low) { (high), (low) }
#define uint64hval(n) ((n).high)
#define uint64lval(n) ((n).low)
static uint64 uint64mk(uint32, uint32);
static uint1 uint64iszero(uint64);
static uint1 uint64le(uint64, uint64);
#define uint64lt(a, b) (!uint64le((b), (a)))
#define uint64gt(a, b) (!uint64le((a), (b)))
#define uint64ge(a, b) (uint64le((b), (a)))
static uint1 uint64eq(uint64, uint64);
#define uint64ne(a, b) (!uint64eq((a), (b)))
static uint64 uint64inc(uint64);
static uint64 uint64dec(uint64);
static uint64 uint64add(uint64, uint64);
static uint64 uint64sub(uint64, uint64);
static uint64 uint64mul(uint64, uint32);
static uint64 uint64div(uint64, uint32);
static uint32 uint64mod(uint64, uint32);
static char const *uint64str(uint64, uint16, uint16);
static uint32 strtouint32(char const *, char **, uint16);
static uint64 strtouint64(char const *, char **, uint16);

/* The length of one quadcent year, 12622780800 / 400 == 31556952 seconds. */
#define QCYEAR 31556952UL
#define STDYEAR 31536000UL

/* Definitions to help with leap years. */
static uint16 nrmdays[12]={ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static uint16 lyrdays[12]={ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
#define jleapyear(y) ( !((y)%4L) )
#define gleapyear(y) ( !((y)%4L) && ( ((y)%100L) || !((y)%400L) ) )
#define jdays(y) (jleapyear(y) ? lyrdays : nrmdays)
#define gdays(y) (gleapyear(y) ? lyrdays : nrmdays)
#define xdays(gp, y) (((gp) ? gleapyear(y) : jleapyear(y)) ? lyrdays : nrmdays)

/* The date 0323-01-01 (0323*01*01) is 117609 days after the internal   *
 * epoch, 0001=01=01 (0000-12-30).  This is a difference of             *
 * 117609*86400 (0x1cb69*0x15180) == 10161417600 (0x25daaed80) seconds. */
static uint64 const qcepoch = UINT64INIT(0x2UL, 0x5daaed80UL);

/* The length of four centuries, 146097 days of 86400 seconds, is *
 * 12622780800 (0x2f0605980) seconds.                             */
static uint64 const quadcent = UINT64INIT(0x2UL, 0xf0605980UL);

/* The epoch for Unix time, 1970-01-01, is 719164 (0xaf93c) days after *
 * our internal epoch, 0001=01=01 (0000-12-30).  This is a difference  *
 * of 719164*86400 (0xaf93c*0x15180) == 62135769600 (0xe77949a00)      *
 * seconds.                                                            */
static uint64 const unixepoch = UINT64INIT(0xeUL, 0x77949a00UL);

/* The epoch for stardates, 2162-01-04, is 789294 (0xc0b2e) days after *
 * the internal epoch.  This is 789294*86400 (0xc0b2e*0x15180) ==      *
 * 68195001600 (0xfe0bd2500) seconds.                                  */
static uint64 const ufpepoch = UINT64INIT(0xfUL, 0xe0bd2500UL);

/* The epoch for TNG-style stardates, 2323-01-01, is 848094 (0xcf0de) *
 * days after the internal epoch.  This is 73275321600 (0x110f8cad00) *
 * seconds.                                                           */
static uint64 const tngepoch = UINT64INIT(0x11UL, 0x0f8cad00UL);

struct caldate {
  uint64 year;
  uint16 month, day;
  uint16 hour, min, sec;
};
static uint16 readcal(struct caldate *, char const *, char);

static uint16 sdin(char const *date, intdate *dt)
{
  static uint64 const nineteen = UINT64INIT(0UL, 19UL);
  static uint64 const twenty = UINT64INIT(0UL, 20UL);
  uint64 nissue;
  uint32 integer, frac;
  char const *cptr = date;
  char *ptr;
  errnot oerrno;
  uint1 negi;
  char fracbuf[7];
  if(*cptr++ != '[')
    return 0;
  if(negi = (*cptr == '-'))
    cptr++;
  if(!ISDIGIT(*cptr))
    return 0;
  errno = 0;
  nissue = strtouint64(cptr, &ptr, 10);
  oerrno = errno;
  if(*ptr++ != ']' || !ISDIGIT(*ptr))
    return 0;
  integer = strtouint32(ptr, &ptr, 10);
  if(errno || integer > 99999UL ||
      (!negi && uint64eq(nissue, twenty) && integer > 5005UL) ||
      ((negi || uint64lt(nissue, twenty)) && integer > 9999UL)) {
    fprintf(stderr, "%s: integer part is out of range: %s\n", progname, date);
    return 2;
  }
  if(*ptr == '.') {
    char *b = fracbuf;
    strcpy(fracbuf, "000000");
    ptr++;
    while(*b && ISDIGIT(*ptr))
      *b++ = *ptr++;
    while(ISDIGIT(*ptr))
      ptr++;
    if(*ptr)
      return 0;
  } else if(*ptr)
    return 0;
  frac = strtouint32(fracbuf, NULL, 10);
  errno = oerrno;
  if(negi || uint64le(nissue, twenty)) {
    /* Pre-TNG stardate */
    uint64 f;
    if(!negi) {
      /* There are two changes in stardate rate to handle: *
       *       up to [19]7340      0.2 days/unit           *
       * [19]7340 to [19]7840     10   days/unit           *
       * [19]7840 to [20]5006      2   days/unit           *
       * we scale to the first of these.                   */
      if(uint64eq(nissue, twenty)) {
	nissue = nineteen;
	integer += 10000UL;
	goto fiddle;
      } else if(uint64eq(nissue, nineteen) && integer >= 7340UL) {
	fiddle:
	/* We have a stardate in the range [19]7340 to [19]15006.  First *
	 * we scale it to match the prior rate, so this range changes to *
	 * 7340 to 390640.                                               */
	integer = 7340UL + ((integer - 7340UL) * 50) + frac / (1000000UL/50);
	frac = (frac * 50UL) % 1000000UL;
	/* Next, if the stardate is greater than what was originally     *
	 * [19]7840 (now represented as 32340), it is in the 2 days/unit *
	 * range, so scale it back again.  The range affected, 32340 to  *
	 * 390640, changes to 32340 to 104000.                           */
	if(integer >= 32340UL) {
	  frac = frac/5UL + (integer%5UL) * (1000000UL/5);
	  integer = 32340UL + (integer - 32340UL) / 5;
	}
	/* The odd stardate has now been scaled to match the early stardate *
	 * type.  It could be up to [19]104000.  Fortunately this will not  *
	 * cause subsequent calculations to overflow.                       */
      }
      dt->sec = uint64add(ufpepoch, uint64mul(nissue, 2000UL*86400UL));
    } else {
      /* Negative stardate.  In order to avoid underflow in some cases, we *
       * actually calculate a date one issue (2000 days) too late, and     *
       * then subtract that much as the last stage.                        */
      dt->sec = uint64sub(ufpepoch,
	  uint64mul(uint64dec(nissue), 2000UL*86400UL));
    }
    dt->sec = uint64add(dt->sec, uint64mk(0, (86400UL/5UL) * integer));
    /* frac is scaled such that it is in the range 0-999999, and a value *
     * of 1000000 would represent 86400/5 seconds.  We want to put frac  *
     * in the top half of a uint64, multiply by 86400/5 and divide by    *
     * 1000000, in order to leave the uint64 containing (top half) a     *
     * number of seconds and (bottom half) a fraction.  In order to      *
     * avoid overflow, this scaling is cancelled down to a multiply by   *
     * 54 and a divide by 3125.                                          */
    f = uint64mul(uint64mk(frac, 0), 54UL);
    f = uint64div(uint64add(f, uint64mk(0, 3124UL)), 3125UL);
    dt->sec = uint64add(dt->sec, uint64mk(0, uint64hval(f)));
    dt->frac = uint64lval(f);
    if(negi) {
      /* Subtract off the issue that was added above. */
      dt->sec = uint64sub(dt->sec, uint64mk(0, 2000UL*86400UL));
    }
  } else {
    uint64 t;
    /* TNG stardate */
    nissue = uint64sub(nissue, uint64mk(0, 21UL));
    /* Each issue is 86400*146097/4 seconds long. */
    dt->sec = uint64add(tngepoch, uint64mul(nissue, (86400UL/4UL)*146097UL));
    /* 1 unit is (86400*146097/4)/100000 seconds, which isn't even. *
     * It cancels to 27*146097/125.                                 */
    t = uint64mul(uint64mk(0, integer), 1000000UL);
    t = uint64add(t, uint64mk(0, frac));
    t = uint64mul(t, 27UL*146097UL);
    dt->sec = uint64add(dt->sec, uint64div(t, 125000000UL));
    t = uint64mk(uint64mod(t, 125000000UL), 0UL);
    t = uint64div(uint64add(t, uint64mk(0, 124999999UL)), 125000000UL);
    dt->frac = uint64lval(t);
  }
  return 1;
}

static uint16 calin(char const *, intdate *, uint1);

static uint16 julin(char const *date, intdate *dt)
{
  return calin(date, dt, 0);
}

static uint16 gregin(char const *date, intdate *dt)
{
  return calin(date, dt, 1);
}

static uint16 calin(char const *date, intdate *dt, uint1 gregp)
{
  struct caldate c;
  uint64 t;
  uint1 low;
  uint16 cycle;
  uint16 n = readcal(&c, date, gregp ? '-' : '=');
  if(n != 1)
    return n;
  cycle = uint64mod(c.year, 400UL);
  if(c.day > xdays(gregp, cycle)[c.month - 1]) {
    fprintf(stderr, "%s: day is out of range: %s\n", progname, date);
    return 2;
  }
  if(low = (gregp && uint64iszero(c.year)))
    c.year = uint64mk(0, 399UL);
  else
    c.year = uint64dec(c.year);
  t = uint64mul(c.year, 365UL);
  if(gregp) {
    t = uint64sub(t, uint64div(c.year, 100UL));
    t = uint64add(t, uint64div(c.year, 400UL));
  }
  t = uint64add(t, uint64div(c.year, 4UL));
  n = 2*(uint16)gregp + c.day - 1;
  for(c.month--; c.month--; )
    n += xdays(gregp, cycle)[c.month];
  t = uint64add(t, uint64mk(0, n));
  if(low)
    t = uint64sub(t, uint64mk(0, 146097UL));
  t = uint64mul(t, 86400UL);
  dt->sec = uint64add(t, uint64mk(0, c.hour*3600UL + c.min*60UL + c.sec));
  dt->frac = 0;
  return 1;
}

static uint16 qcin(char const *date, intdate *dt)
{
  struct caldate c;
  uint64 secs, t, f;
  uint1 low;
  uint16 n = readcal(&c, date, '*');
  if(n != 1)
    return n;
  if(c.day > nrmdays[c.month - 1]) {
    fprintf(stderr, "%s: day is out of range: %s\n", progname, date);
    return 2;
  }
  if(low = uint64lt(c.year, uint64mk(0, 323UL)))
    c.year = uint64add(c.year, uint64mk(0, 400UL - 323UL));
  else
    c.year = uint64sub(c.year, uint64mk(0, 323UL));
  secs = uint64add(qcepoch, uint64mul(c.year, QCYEAR));
  for(n = c.day - 1, c.month--; c.month--; )
    n += nrmdays[c.month];
  t = uint64mk(0, n * 86400UL + c.hour * 3600UL + c.min * 60UL + c.sec);
  t = uint64mul(t, QCYEAR);
  f = uint64mk(uint64mod(t, STDYEAR), STDYEAR - 1);
  secs = uint64add(secs, uint64div(t, STDYEAR));
  if(low)
    secs = uint64sub(secs, quadcent);
  dt->sec = secs;
  dt->frac = uint64lval(uint64div(f, STDYEAR));
  return 1;
}

static uint16 readcal(struct caldate *c, char const *date, char sep)
{
  errnot oerrno;
  uint32 ul;
  char *ptr;
  char const *pos = date;
  if(!ISDIGIT(*pos))
    return 0;
  while(ISDIGIT(*++pos));
  if(*pos++ != sep || !ISDIGIT(*pos))
    return 0;
  while(ISDIGIT(*++pos));
  if(*pos++ != sep || !ISDIGIT(*pos))
    return 0;
  while(ISDIGIT(*++pos));
  if(*pos) {
    if((*pos != 'T' && *pos != 't') || !ISDIGIT(*++pos)) {
      badtime:
      fprintf(stderr, "%s: malformed time of day: %s\n", progname, date);
      return 2;
    }
    while(ISDIGIT(*++pos));
    if(*pos++ != ':' || !ISDIGIT(*pos))
      goto badtime;
    while(ISDIGIT(*++pos));
    if(*pos) {
      if(*pos++ != ':' || !ISDIGIT(*pos))
	goto badtime;
      while(ISDIGIT(*++pos));
      if(*pos)
	goto badtime;
    }
  }
  errno = 0;
  c->year = strtouint64(date, &ptr, 10);
  oerrno = errno;
  errno = 0;
  ul = strtouint32(ptr+1, &ptr, 10);
  if(errno || !ul || ul > 12UL) {
    fprintf(stderr, "%s: month is out of range: %s\n", progname, date);
    return 2;
  }
  c->month = ul;
  ul = strtouint32(ptr+1, &ptr, 10);
  if(errno || !ul || ul > 31UL) {
    fprintf(stderr, "%s: day is out of range: %s\n", progname, date);
    return 2;
  }
  c->day = ul;
  if(!*ptr) {
    c->hour = c->min = c->sec = 0;
    errno = oerrno;
    return 1;
  }
  ul = strtouint32(ptr+1, &ptr, 10);
  if(errno || ul > 23UL) {
    fprintf(stderr, "%s: hour is out of range: %s\n", progname, date);
    return 2;
  }
  c->hour = ul;
  ul = strtouint32(ptr+1, &ptr, 10);
  if(errno || ul > 59UL) {
    fprintf(stderr, "%s: minute is out of range: %s\n", progname, date);
    return 2;
  }
  c->min = ul;
  if(!*ptr) {
    c->sec = 0;
    errno = oerrno;
    return 1;
  }
  ul = strtouint32(ptr+1, &ptr, 10);
  if(errno || ul > 59UL) {
    fprintf(stderr, "%s: second is out of range: %s\n", progname, date);
    return 2;
  }
  c->sec = ul;
  errno = oerrno;
  return 1;
}

static uint16 unixin(char const *date, intdate *dt)
{
  char const *pos = date+1;
  uint16 radix = 10;
  uint1 neg;
  char *ptr;
  uint64 mag;
  if(date[0] != 'u' && date[0] != 'U')
    return 0;
  pos += neg = *pos == '-';
  if(pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X')) {
    pos += 2;
    radix = 16;
  }
  if(!ISALNUM(*pos)) {
    bad:
    fprintf(stderr, "%s: malformed Unix date: %s\n", progname, date);
    return 2;
  }
  mag = strtouint64(pos, &ptr, radix);
  if(*ptr)
    goto bad;
  dt->sec = (neg ? uint64sub : uint64add)(unixepoch, mag);
  dt->frac = 0;
  return 1;
}

static char const *tngsdout(intdate const *);

static char const *sdout(intdate const *dt)
{
  uint1 isneg;
  uint32 nissue, integer;
  uint64 frac;
  static char ret[18];
  if(uint64le(tngepoch, dt->sec))
    return tngsdout(dt);
  if(uint64lt(dt->sec, ufpepoch)) {
    /* Negative stardate */
    uint64 diff = uint64dec(uint64sub(ufpepoch, dt->sec));
    uint32 nsecs = 2000UL*86400UL - 1 - uint64mod(diff, 2000UL * 86400UL);
    isneg = 1;
    nissue = 1 + uint64lval(uint64div(diff, 2000UL * 86400UL));
    integer = nsecs / (86400UL/5);
    frac = uint64mul(uint64mk(nsecs % (86400UL/5), dt->frac), 50UL);
  } else if(uint64lt(dt->sec, tngepoch)) {
    /* Positive stardate */
    uint64 diff = uint64sub(dt->sec, ufpepoch);
    uint32 nsecs = uint64mod(diff, 2000UL * 86400UL);
    isneg = 0;
    nissue = uint64lval(uint64div(diff, 2000UL * 86400UL));
    if(nissue < 19 || (nissue == 19 && nsecs < 7340UL * (86400UL/5))) {
      /* TOS era */
      integer = nsecs / (86400UL/5);
      frac = uint64mul(uint64mk(nsecs % (86400UL/5), dt->frac), 50UL);
    } else {
      /* Film era */
      nsecs += (nissue - 19) * 2000UL*86400UL;
      nissue = 19;
      nsecs -= 7340UL * (86400UL/5);
      if(nsecs >= 5000UL*86400UL) {
	/* Late film era */
	nsecs -= 5000UL*86400UL;
	integer = 7840 + nsecs/(86400UL*2);
	if(integer >= 10000) {
	  integer -= 10000;
	  nissue++;
	}
	frac = uint64mul(uint64mk(nsecs % (86400UL*2), dt->frac), 5UL);
      } else {
	/* Early film era */
	integer = 7340 + nsecs/(86400UL*10);
	frac = uint64mk(nsecs % (86400UL*10), dt->frac);
      }
    }
  }
  sprintf(ret, "[%s%lu]%04lu", "-" + !isneg, nissue, integer);
  if(sddigits) {
    char *ptr = strchr(ret, 0);
    /* At this point, frac is a fractional part of a unit, in the range *
     * 0 to (2^32 * 864000)-1.  In order to represent this as a 6-digit *
     * decimal fraction, we need to scale this.  Mathematically, we     *
     * need to multiply by 1000000 and divide by (2^32 * 864000).  But  *
     * multiplying by 1000000 would cause overflow.  Cancelling the two *
     * values yields an algorithm of multiplying by 125 and dividing by *
     * (2^32*108).                                                      */
    frac = uint64div(uint64mul(frac, 125UL), 108UL);
    sprintf(ptr, ".%06lu", uint64hval(frac));
    ptr[sddigits + 1] = 0;
  }
  return ret;
}

static char const *tngsdout(intdate const *dt)
{
  static char ret[UINT64_DIGITS + 15];
  uint64 h, l;
  uint32 nsecs;
  uint64 diff = uint64sub(dt->sec, tngepoch);
  /* 1 issue is 86400*146097/4 seconds long, which just fits in 32 bits. */
  uint64 nissue = uint64add(uint64mk(0, 21UL),
      uint64div(diff, (86400UL/4)*146097UL));
  nsecs = uint64mod(diff, (86400UL/4)*146097UL);
  /* 1 unit is (86400*146097/4)/100000 seconds, which isn't even. *
   * It cancels to 27*146097/125.  For a six-figure fraction,     *
   * divide that by 1000000.                                      */
  h = uint64mul(uint64mk(0, nsecs), 125000000UL);
  l = uint64mul(uint64mk(0, dt->frac), 125000000UL);
  h = uint64add(h, uint64mk(0, uint64hval(l)));
  h = uint64div(h, 27UL*146097UL);
  sprintf(ret, "[%s]%05lu", uint64str(nissue, 10, 1),
      uint64lval(uint64div(h, 1000000UL)));
  if(sddigits) {
    char *ptr = strchr(ret, 0);
    sprintf(ptr, ".%06lu", uint64mod(h, 1000000UL));
    ptr[sddigits + 1] = 0;
  }
  return ret;
}

static char const *calout(intdate const *, uint1);

static char const *julout(intdate const *dt)
{
  return calout(dt, 0);
}

static char const *gregout(intdate const *dt)
{
  return calout(dt, 1);
}

static char const *docalout(char, uint1, uint16, uint64, uint16, uint32);

static char const *calout(intdate const *dt, uint1 gregp)
{
  uint32 tod = uint64mod(dt->sec, 86400UL);
  uint64 year, days = uint64div(dt->sec, 86400UL);
  /* We need the days number to be days since an xx01.01.01 to get the *
   * leap year cycle right.  For the Julian calendar, it is already    *
   * so (0001=01=01).  But for the Gregorian calendar, the epoch is    *
   * 0000-12-30, so we must add on 400 years minus 2 days.  The year   *
   * number gets corrected below.                                      */
  if(gregp)
    days = uint64add(days, uint64mk(0, 146095UL));
  /* Approximate the year number, underestimating but only by a limited *
   * amount.  days/366 is a first approximation, but it goes out by 1   *
   * day every non-leap year, and so will be a full year out after 366  *
   * non-leap years.  In the Julian calendar, we get 366 non-leap years *
   * every 488 years, so adding (days/366)/487 corrects for this.  In   *
   * the Gregorian calendar, it is not so simple: we get 400 years      *
   * every 146097 days, and then add on days/366 within that set of 400 *
   * years.                                                             */
  if(gregp)
    year = uint64add(uint64mul(uint64div(days, 146097UL), 400UL),
	uint64mk(0, uint64mod(days, 146097UL) / 366UL));
  else
    year = uint64add(uint64div(days, 366UL), uint64div(days, 366UL * 487UL));
  /* We then adjust the number of days remaining to match this *
   * approximation of the year.  Note that this approximation  *
   * will never be more than two years off the correct date,   *
   * so the number of days left no longer needs to be stored   *
   * in a uint64.                                              */
  if(gregp)
    days = uint64sub(uint64add(days, uint64div(year, 100UL)),
	uint64div(year, 400UL));
  days = uint64sub(days,
      uint64add(uint64mul(year, 365UL), uint64div(year, 4UL)));
  /* Now correct the year to an actual year number (see notes above). */
  if(gregp)
    year = uint64sub(year, uint64mk(0, 399UL));
  else
    year = uint64inc(year);
  return docalout(gregp ? '-' : '=', gregp, uint64mod(year, 400UL),
      year, uint64lval(days), tod);
}

static char const *docalout(char sep, uint1 gregp, uint16 cycle,
    uint64 year, uint16 ndays, uint32 tod)
{
  uint16 nmonth = 0;
  uint16 hr, min, sec;
  static char ret[UINT64_DIGITS + 16];
  /* Walk through the months, fixing the year, and as a side effect *
   * calculating the month number and day of the month.             */
  while(ndays >= xdays(gregp, cycle)[nmonth]) {
    ndays -= xdays(gregp, cycle)[nmonth];
    if(++nmonth == 12) {
      nmonth = 0;
      year = uint64inc(year);
      cycle++;
    }
  }
  ndays++;
  nmonth++;
  /* Now sort out the time of day. */
  hr = tod / 3600;
  tod %= 3600;
  min = tod / 60;
  sec = tod % 60;
  sprintf(ret, "%s%c%02d%c%02dT%02d:%02d:%02d",
      uint64str(year, 10, 4), sep, nmonth, sep, ndays, hr, min, sec);
  return ret;
}

static char const *qcout(intdate const *dt)
{
  uint64 secs = dt->sec;
  uint32 nsec;
  uint64 year, h, l;
  uint1 low;
  if(low = uint64lt(secs, qcepoch))
    secs = uint64add(secs, quadcent);
  secs = uint64sub(secs, qcepoch);
  nsec = uint64mod(secs, QCYEAR);
  secs = uint64div(secs, QCYEAR);
  if(low)
    year = uint64sub(secs, uint64mk(0, 400 - 323));
  else
    year = uint64add(secs, uint64mk(0, 323));
  /* We need to translate the nsec:dt->frac value (real seconds up to *
   * 31556952:0) into quadcent seconds.  This can be done by          *
   * multiplying by 146000 and dividing by 146097.  Normally this     *
   * would overflow, so we do this in two parts.                      */
  h = uint64mk(0, nsec);
  l = uint64mk(0, dt->frac);
  h = uint64mul(h, 146000);
  l = uint64mul(l, 146000);
  h = uint64add(h, uint64mk(0, uint64hval(l)));
  nsec = uint64lval(uint64div(h, 146097));
  return docalout('*', 0, 1, year, nsec / 86400, nsec % 86400UL);
}

static char const *unixout(intdate const *, uint16, char const *);

static char const *unixdout(intdate const *dt)
{
  return unixout(dt, 10, "");
}

static char const *unixxout(intdate const *dt)
{
  return unixout(dt, 16, "0x");
}

static char const *unixout(intdate const *dt, uint16 radix, char const *prefix)
{
  static char ret[UINT64_DIGITS + 3];
  char const *sgn;
  uint64 mag;
  if(uint64le(unixepoch, dt->sec)) {
    sgn = "";
    mag = uint64sub(dt->sec, unixepoch);
  } else {
    sgn = "-";
    mag = uint64sub(unixepoch, dt->sec);
  }
  sprintf(ret, "U%s%s%s", sgn, prefix, uint64str(mag, radix, 1));
  return ret;
}

static uint64 uint64mk(uint32 h, uint32 l)
{
  uint64 r;
  r.high = h;
  r.low = l;
  return r;
}

static uint1 uint64iszero(uint64 n)
{
  return !n.high && !n.low;
}

static uint1 uint64le(uint64 a, uint64 b)
{
  return a.high < b.high || (a.high == b.high && a.low <= b.low);
}

static uint1 uint64eq(uint64 a, uint64 b)
{
  return a.low == b.low && a.high == b.high;
}

static uint64 uint64inc(uint64 n)
{
  n.low++;
  if(!(n.low &= 0xffffffffUL)) {
    n.high = (n.high+1) & 0xffffffffUL;
    if(!n.high)
      errno = ERANGE;
  }
  return n;
}

static uint64 uint64dec(uint64 n)
{
  if(!n.low) {
    if(!n.high) {
      n.high = 0xffffffffUL;
      errno = ERANGE;
    } else
      n.high--;
    n.low = 0xffffffffUL;
  } else
    n.low--;
  return n;
}

static uint64 uint64add(uint64 a, uint64 b)
{
  uint64 r;
  r.low = (a.low + b.low) & 0xffffffffUL;
  r.high = (a.high + b.high + (r.low < a.low)) & 0xffffffffUL;
  if(r.high < a.high || r.high < b.high)
    errno = ERANGE;
  return r;
}

static uint64 uint64sub(uint64 a, uint64 b)
{
  uint64 r;
  r.high = (a.high - b.high - (a.low < b.low)) & 0xffffffffUL;
  if(b.high + (a.low < b.low) > a.high)
    errno = ERANGE;
  r.low = (a.low - b.low) & 0xffffffffUL;
  return r;
}

static uint64 uint64mul(uint64 vl, uint32 mul)
{
  uint64 r;
  uint16 ml = mul & 0xffffU, mh = mul >> 16;
  uint16 ll = vl.low & 0xffffU, lh = vl.low >> 16;
  uint32 rl = (uint32)ll * ml;
  uint32 rm = (rl >> 16) + (uint32)ll*mh + (uint32)lh*ml;
  rl &= 0xffffU;
  r.high = (vl.high*mul + (uint32)lh*mh + (rm >> 16)) & 0xffffffffUL;
  if(vl.high*mul/mul != vl.high || r.high < vl.high*mul || r.high < (uint32)lh*mh)
    errno = ERANGE;
  rm &= 0xffffU;
  r.low = (rm << 16) | rl;
  return r;
}

static uint64 uint64div(uint64 n, uint32 div)
{
  uint64 r, m;
  uint32 b = 0x80000000UL;
  r.high = n.high / div;
  r.low = 0;
  n.high %= div;
  m.high = div;
  m.low = 0;
  while(n.high) {
    m.low = (m.low >> 1) | ((m.high&1) << 31);
    m.high >>= 1;
    if(uint64le(m, n)) {
      n = uint64sub(n, m);
      r.low |= b;
    }
    b >>= 1;
  }
  r.low |= n.low / div;
  return r;
}

static uint32 uint64mod(uint64 n, uint32 div)
{
  uint64 m;
  n.high %= div;
  m.high = div;
  m.low = 0;
  while(n.high) {
    m.low = (m.low >> 1) | ((m.high&1) << 31);
    m.high >>= 1;
    if(m.high < n.high || (m.high == n.high && m.low <= n.low))
      n = uint64sub(n, m);
  }
  return n.low % div;
}

static char const xdigits[] = "0123456789abcdef";
static char const cdigits[] = "0123456789ABCDEF";

static char const *uint64str(uint64 n, uint16 radix, uint16 min)
{
  static char ret[UINT64_DIGITS + 1];
  char *pos = ret + UINT64_DIGITS, *end = pos - min;
  *pos = 0;
  while(n.high || n.low) {
    *--pos = xdigits[uint64mod(n, radix)];
    n = uint64div(n, radix);
  }
  while(pos > end)
    *--pos = '0';
  return pos;
}

static uint32 strtouint32(char const *str, char **ptr, uint16 radix)
{
  errnot oerrno = errno;
  uint32 n = 0;
  for(; *str; str++) {
    char const *d;
    uint16 v;
    if((d = strchr(xdigits, *str)))
      v = d - xdigits;
    else if((d = strchr(cdigits, *str)))
      v = d - cdigits;
    else
      break;
    if(n > (UINT32_HIGH-v) / radix)
      oerrno = ERANGE;
    n = n*radix + v;
  }
  errno = oerrno;
  if(ptr)
    *ptr = (char *)str;
  return n;
}

static uint64 strtouint64(char const *str, char **ptr, uint16 radix)
{
  errnot oerrno = errno;
  uint64 n = UINT64INIT(0, 0);
  for(; *str; str++) {
    char const *d;
    uint16 v;
    if((d = strchr(xdigits, *str)))
      v = d - xdigits;
    else if((d = strchr(cdigits, *str)))
      v = d - cdigits;
    else
      break;
    errno = 0;
    n = uint64add(uint64mul(n, radix), uint64mk(0, v));
    if(errno)
      oerrno = errno;
  }
  errno = oerrno;
  if(ptr)
    *ptr = (char *)str;
  return n;
}
