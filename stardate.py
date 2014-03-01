#!/usr/bin/env python
#
# Author: Rahul Anand <et@eternal-thinker.com>
#         2014-02-21, stardate[-28]( <date> 6.00 pm 30 secs)            
#
# Description: 
#   Convert date formats to Stardates
#   Uses Stardate versions in Star Trek FAQ, as adopted by Google Calender 
#
# Python version: 2.7
#####################################################################################
#
# Based on the following work:
#
# stardate: convert between date formats
# by Andrew Main <zefram@fysh.org>
# 1997-12-26, stardate [-30]0458.96
#
# Stardate code is based on version 1 of the Stardates in Star Trek FAQ.
#####################################################################################

import time

# /* The date 0323-01-01 (0323*01*01) is 117609 days after the internal   *
# * epoch, 0001=01=01 (0000-12-30).  This is a difference of             *
# * 117609*86400 (0x1cb69*0x15180) == 10161417600 (0x25daaed80) seconds. */
qcepoch = 0x25daaed80    

# /* The length of four centuries, 146097 days of 86400 seconds, is *
# * 12622780800 (0x2f0605980) seconds.                             */
quadcent = 0x2f0605980

# /* The epoch for Unix time, 1970-01-01, is 719164 (0xaf93c) days after *
# * our internal epoch, 0001=01=01 (0000-12-30).  This is a difference  *
# * of 719164*86400 (0xaf93c*0x15180) == 62135769600 (0xe77949a00)      *
# * seconds.                                                            */
unixepoch = 0xe77949a00

# /* The epoch for stardates, 2162-01-04, is 789294 (0xc0b2e) days after *
# * the internal epoch.  This is 789294*86400 (0xc0b2e*0x15180) ==      *
# * 68195001600 (0xfe0bd2500) seconds.                                  */
ufpepoch = 0xfe0bd2500

# /* The epoch for TNG-style stardates, 2323-01-01, is 848094 (0xcf0de) *
# * days after the internal epoch.  This is 73275321600 (0x110f8cad00) *
# * seconds.                                                           */
tngepoch = 0x110f8cad00

class Stardate():

    # The length of one quadcent year, 12622780800 / 400 == 31556952 seconds. 
    QCYEAR = 31556952
    STDYEAR = 31536000

    # Definitions to help with leap years. */
    nrmdays = [ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 ]
    lyrdays = [ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 ]

    def jleapyear(self, y):
        return not y % 4

    def gleapyear(self, y):
        return (not y % 4) and (y % 100 or not y % 400)

    def jdays(self, y):
        return lyrdays if jleapyear(y) else nrmdays

    def gdays(self, y):
        return lyrdays if gleapyear(y) else nrmdays

    def xdays(self, gp, y):
        return lyrdays if (gleapyear(y) if gp else jleapyear(y)) else nrmdays

    #define jleapyear(y) ( !((y)%4L) )
    #define gleapyear(y) ( !((y)%4L) && ( ((y)%100L) || !((y)%400L) ) )
    #define jdays(y) (jleapyear(y) ? lyrdays : nrmdays)
    #define gdays(y) (gleapyear(y) ? lyrdays : nrmdays)
    #define xdays(gp, y) ( ((gp) ? gleapyear(y) : jleapyear(y))    ? lyrdays : nrmdays)

    

    def __init__(self):
        pass

    def toStardate(self, date=None):
        if not date:
            date = time.strftime("%d %m %Y %H %M %S")
        d, m, y, H, M, S = [ int(item) for item in date.split() ]
        S = int(time.time())

        print "S: ", str(S)
        
        isneg = True
        nissue, integer, frac = 0, 0, 0

        if S > tngepoch:
            return toTngStardate(date)

        if S < ufpepoch: 
            print "negative"
            # negative stardate
            diff = ufpepoch - S
            print str(diff)
            #? diff -= 1
            nsecs = 2000*86400 - 1 - (diff % (2000 * 86400))
            isneg = True
            nissue = 1 + ((diff / (2000 * 86400)) & 0xffffffff)
            integer = nsecs / (86400/5)
            frac = ( ((nsecs % (86400/5)) << 32) & S ) * 50
        elif S < tngepoch: 
            # positive stardate
            diff = S - ufpepoch
            nsecs = diff % (2000 * 86400)
            isneg = False
            nissue = (diff / (2000 * 86400)) & 0xffffffff

            if nissue < 19 or ( nissue == 19 and nsecs < (7340*(86400/5)) ) : 
                # TOS era
                integer = nsecs / (86400/5)
                frac = ( ((nsecs % (86400/5)) << 32) & S ) * 50
            else:
                # film era
                nsecs += (nissue - 19) * 2000 * 86400
                nissue = 19
                nsecs -= 7340 * (86400/5)
                if nsecs >= 5000*86400:
                    # late film era
                    nsecs -= 5000 * 86400
                    integer = 7840 + nsecs / (86400*2)
                    if integer >= 10000:
                        integer -= 10000
                        nissue += 1
                    frac = ( ((nsecs % (86400*2)) << 32) & S ) * 5
                else:
                    # early film era
                    integer = 7340 + nsecs / (86400*10)
                    frac = ( ((nsecs % (86400*10)) << 32) & S )

        ret = "[" + ("-" if isneg else "")  + str(nissue) + "]" + str(integer) 
        frac = ( ( ((frac * 125) / 108) >> 32 ) & 0xffffffff ) # round
        ret += "." + str(frac)
        return ret

    def toTngStardate(self, date=None):
        return "TNG stardate"   

    def getcurdate(self):
        pass

    def gregin(self):
        struct caldate c;
        uint64 t;
        uint1 low;
        uint16 cycle;
        
        d, m, y, H, M, S = 0, 0, 0, 0, 0, 0
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

    def fromStardate(self, stardate):
        nineteen = [0, 19];
        twenty = [0, 20];        
       

if __name__ == "__main__":    
    sd = Stardate()
    print "Current stardate is: %s" % sd.toStardate()