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
# Copyright (c) 1996, 1997 Andrew Main.  All rights reserved.
#
# Stardate code is based on version 1 of the Stardates in Star Trek FAQ.
#####################################################################################

import sys
import datetime

# The date 0323-01-01 (0323*01*01) is 117609 days after the internal
# epoch, 0001=01=01 (0000-12-30).  This is a difference of
# 117609*86400 (0x1cb69*0x15180) == 10161417600 (0x25daaed80) seconds.
qcepoch = 0x25daaed80

# The length of four centuries, 146097 days of 86400 seconds, is
# 12622780800 (0x2f0605980) seconds.
quadcent = 0x2f0605980

# The epoch for Unix time, 1970-01-01, is 719164 (0xaf93c) days after
# our internal epoch, 0001=01=01 (0000-12-30).  This is a difference
# of 719164*86400 (0xaf93c*0x15180) == 62135769600 (0xe77949a00)
# seconds.
unixepoch = 0xe77949a00

# The epoch for stardates, 2162-01-04, is 789294 (0xc0b2e) days after
# the internal epoch.  This is 789294*86400 (0xc0b2e*0x15180) ==
# 68195001600 (0xfe0bd2500) seconds.
ufpepoch = 0xfe0bd2500

# The epoch for TNG-style stardates, 2323-01-01, is 848094 (0xcf0de)
# days after the internal epoch.  This is 73275321600 (0x110f8cad00)
# seconds.
tngepoch = 0x110f8cad00

class Stardate():

    # The length of one quadcent year, 12622780800 / 400 == 31556952 seconds.
    QCYEAR = 31556952
    STDYEAR = 31536000

    # Definitions to help with leap years.
    nrmdays = [ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 ]
    lyrdays = [ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 ]

    def jleapyear(self, y):
        return not y % 4

    def gleapyear(self, y):
        return (not y % 4) and (y % 100 or not y % 400)

    def jdays(self, y):
        return self.lyrdays if self.jleapyear(y) else self.nrmdays

    def gdays(self, y):
        return self.lyrdays if self.gleapyear(y) else self.nrmdays

    def xdays(self, gp, y):
        return self.lyrdays if (self.gleapyear(y) if gp else self.jleapyear(y)) else self.nrmdays

    def __init__(self):
        pass

    def toStardate(self, date=None):
        S = 0
        F = 0
        if not date:
            date = self.getcurdate()
        S = self.gregin(date)

        isneg = True
        nissue, integer, frac = 0, 0, 0

        if S >= tngepoch:
            return self.toTngStardate(S, F)

        if S < ufpepoch:
            # negative stardate
            diff = ufpepoch - S
            nsecs = 2000*86400 - 1 - (diff % (2000 * 86400))
            isneg = True
            nissue = 1 + ((diff / (2000 * 86400)) & 0xffffffff)
            integer = nsecs / (86400/5)
            frac = ( ((nsecs % (86400/5)) << 32) | F ) * 50
        elif S < tngepoch:
            # positive stardate
            diff = S - ufpepoch
            nsecs = diff % (2000 * 86400)
            isneg = False
            nissue = (diff / (2000 * 86400)) & 0xffffffff

            if nissue < 19 or ( nissue == 19 and nsecs < (7340*(86400/5)) ) :
                # TOS era
                integer = nsecs / (86400/5)
                frac = ( ((nsecs % (86400/5)) << 32) | F ) * 50
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
                    frac = ( ((nsecs % (86400*2)) << 32) | F ) * 5
                else:
                    # early film era
                    integer = 7340 + nsecs / (86400*10)
                    frac = ( ((nsecs % (86400*10)) << 32) | F )

        ret = "[" + ("-" if isneg else "")  + str(nissue) + "]" + str(integer).zfill(4)
        frac = ( ( ((frac * 125) / 108) >> 32 ) & 0xffffffff ) # round
        ret += "." + str(frac)
        return ret

    def toTngStardate(self, S = 0, F =0):
        diff = S - tngepoch
        # 1 issue is 86400*146097/4 seconds long, which just fits in 32 bits.
        nissue = 21 + diff/((86400/4)*146097)
        nsecs = diff % ((86400/4)*146097)
        # 1 unit is (86400*146097/4)/100000 seconds, which isn't even.
        # It cancels to 27*146097/125.  For a six-figure fraction,
        # divide that by 1000000.
        h = nsecs * 125000000
        l = F * 125000000
        h = h + ((l >> 32) & 0xffffffff)
        h = h / (27*146097)
        ret = "[%d]%05d" % ( nissue, ((h/1000000) & 0xffffffff) )
        ret += ".%06d" % (h % 1000000)
        return ret;

    def getcurdate(self):
        # use UTC time for now
        date = datetime.datetime.utcnow().strftime("%Y %m %d %H %M %S")
        utc = [ int(item) for item in date.split() ]
        print "utc array:", utc
        return utc

    def gregin(self, date=None):
        y, m, d, H, M, S = date

        cycle = y % 400

        low = (y == 0)
        if low:
            y = 399
        else:
            y = y - 1

        t = y * 365
        t = t - y/100
        t = t + y/400
        t = t + y/4

        n = 2 + d - 1
        m -= 1
        while m > 0:
            n += self.xdays(True, cycle)[m]
            m -= 1

        t = t + n
        if low:
            t = t - 146097
        t = t * 86400

        retS = t + H*3600 + M*60 + S

        return retS


    def fromStardate(self, stardate):
        nineteen = [0, 19];
        twenty = [0, 20];


if __name__ == "__main__":
    sd = Stardate()

    if len(sys.argv) > 1:
        datein = datetime.datetime.strptime(sys.argv[1], "%Y-%m-%d").replace('-', ' ')
        timein = "0 0 0"
        if len(sys.argv) > 2:
            timein = datetime.datetime.strptime(sys.argv[2], "%H-%M-%S").replace(':', ' ')
        date = [ int(item) for item in (datein + " " + timein).split() ]
        #date = [2162, 1, 4, 1, 0, 0] #ufepoch
        #date = [2323, 1, 1, 0, 0, 0] #tngepoch
        print "%s" % sd.toStardate(date)
    else:
        print "%s" % sd.toStardate()
