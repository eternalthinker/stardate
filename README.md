stardate
========

Stardate conversion program following Star Trek FAQ style as adopted in Google Calender

Provides conversion from Gregarian calender dates to Stardates and vice versa

Using in python code:
---------------------
from stardate import Stardate

methods:
  * toStardate([date]) 
      provide date as list [yyyy, mm, dd, HH, MM, SS]
      default action is to process current local time

  * fromStardate(stardate)
      provide stardate as string "[issue]integer.fraction"


Using in commandline:
---------------------
python stardate.py yyyy-mm-dd [HH:MM:SS:]
python stardate.py [issue]integer.fraction
