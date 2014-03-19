stardate
========

Stardate conversion program following Star Trek FAQ style as adopted in Google Calender

Provides conversion from Gregarian calender dates to Stardates and vice versa

Using in python code:
---------------------
```python
from stardate import Stardate  
sd = new Stardate()  
sd.toStardate()
```  
_methods:_
* `toStardate(date=None)`  
Provide date as string "yyyy-mm-dd HH:MM:SS". Default action if no date is provided is to process current local time

* `fromStardate(stardate)`   
Provide stardate as string "[ii]nnnn.ffffff"
  



Using in commandline:
---------------------
```python
python stardate.py yyyy-mm-dd HH:MM:SS  
  
python stardate.py 2014-3-1 12:30:35
python stardate.py 2060-01-01
```  

```python
python stardate.py [ii]nnnn.ffffff  
  
python stardate.py [0]000.00  
python stardate.py [-27]1234.123456  
python stardate.py [19]1234.12  
python stardate.py [-27]1234  
```  

