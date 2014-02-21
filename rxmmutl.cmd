/* REXX Prozedur Sammlung */


IF RxFuncAdd("SysLoadFuncs", "RexxUtil", "SysLoadFuncs") = 0 THEN
   CALL SysLoadFuncs

IF RxFuncAdd("MMLoadFuncs", "RxMMutl", "MMLoadFuncs") = 0 THEN
   CALL MMLoadFuncs


SAY "collection of REXX procedures"


/****************************************************************************
   parse command line
*/
params = STRIP(ARG(1))
nomoreopt = 0
DO WHILE params \= ''
   IF LEFT(params,1) = '"' THEN DO
      PARSE VAR params '"'param'"'params
      END
    ELSE
      PARSE VAR params param params
   params = STRIP(params, 'L')
   /* param is the next parameter to process ... */
   IF (\nomoreopt) & ((LEFT(param, 1) = '/') | (LEFT(param, 1) = '-')) THEN
      SELECT
       WHEN param = '--'
         nomoreopt = 1

         /* parsing options ... */

       OTHERWISE
         SAY "illegal option '"param"'"
         END

    ELSE DO
      /* any other arg ... */
      SAY "arg: "param
      END
   END

EXIT

/****************************************************************************
   UNIX Time
*/

/* convert to unix time
   ToUnixTime(year, month, day, hour, minute, second)
*/
ToUnixtime: PROCEDURE
   year70 = ARG(1) - 1970;
   /* check
   if ( tm->Sec > 59U || tm->Min > 59U || tm->Hour > 23U
       || (unsigned)year70 >= 136U || (tm->Month-1U) > 11U
       || (tm->Day-1) >= monlen[tm->Month-1]
       || (tm->Day == 29 && tm->Month == 2 && (year70&3)) )
      return false;*/
   monsum = "0 31 59 90 120 151 181 212 243 273 304 334 365"
   ysec = ARG(6) + 60*ARG(5) + 3600*ARG(4) + 86400*(ARG(3)-1 + WORD(monsum, ARG(2)) + (ARG(2) >= 3 & year70//4 == 2))
   ydays = 365*year70 + (year70+1)%4
   RETURN ysec + 86400*ydays

GmTime: PROCEDURE
/* convert unix time to 'tm' like structure
   ARG(1)   unix time
   RETURN   int   tm_sec   seconds after the minute [0-59]
            int   tm_min   minutes after the hour [0-59]
            int   tm_hour  hours since midnight [0-23]
            int   tm_mday  day of the month [1-31]
            int   tm_mon   months since January [0-11] - note the zero based offset!
            int   tm_year  years since 1900
            int   tm_wday  days since Sunday [0-6]
            int   tm_yday  days since January 1 [0-365]
*/
   sec = ARG(1) // 60
   t = ARG(1) % 60
   min = t // 60
   t = t % 60
   hour = t // 24
   days = t % 24
   wday = (days+4) // 7
   days4 = days * 4 +2 /* [days*4+2], now the funny part starts ... */
   year = days4 % 1461 + 70
   yday = days4 // 1461 % 4
   t = yday * 12 + 6 /* [remaining days *12 +6] */
   IF year // 4 \= 0 THEN DO
      IF t > 59*12 THEN
         t = t + 2*12
      END
    ELSE IF t > 60*12 THEN
      t = t + 1*12
   mon = t % 367
   day = (t // 367) % 12 +1
   RETURN sec min hour day mon year wday yday

/* return unix time
   However, the time zone is ignored here
*/
UnixTime: PROCEDURE
   RETURN (DATE('B')-719162)*24*60*60 + TIME('S')


/****************************************************************************
   String utilities fr Dateinamen
*/
/* wildcard match
   StringMatchQ(string, template)
*/
StringMatchQ: PROCEDURE
   str = ARG(1)
   tmp = ARG(2)
 redo:
   SAY 'SMQ: "'str'", "'tmp'"'
   IF LENGTH(str) = 0 THEN
      RETURN VERIFY(tmp, '*') = 0 /* '' matches only if template consists only of '*' */
   p = VERIFY(tmp, '?*', 'M')
   /*SAY 'p: 'p*/
   IF p = 0 THEN
      RETURN COMPARE(str, tmp, '*') = 0 /* no wildcards => compare strings */
   IF COMPARE(LEFT(str, p-1), LEFT(tmp, p-1, '*')) \= 0 THEN
      RETURN 0 /* compare of non-wildcard section failed */
   j = p
   DO i = p TO VERIFY(tmp' ', '*?',, p +1) -1 /* count # of ? */
      IF SUBSTR(tmp, i, 1) = '?' THEN
         j = j +1
      END
   /*SAY 'i: 'i', j: 'j*/
   IF j > LENGTH(str)+1 THEN
      RETURN 0 /* more ? than length of str */
   tmp = SUBSTR(tmp, i)
   str = SUBSTR(str, j)
   /*SAY 'str: "'str'", tmp: "'tmp'"'*/
   IF i = j THEN
      SIGNAL redo
   /* '*' */
   IF LENGTH(tmp) = 0 THEN
      RETURN 1 /* nothing after '*' */
   DO p = 1
      p = POS(LEFT(tmp, 1), str, p)
      /*SAY 'p*: 'p*/
      IF p = 0 THEN
         RETURN 0 /* character after '*' not found */
      IF StringMatchQ(SUBSTR(str, p+1), SUBSTR(tmp, 2)) THEN
         RETURN 1 /* got it! */
      END

/* variant 2 */
   IF i \= j THEN DO /* '*' */
      IF LENGTH(tmp) = 0 THEN
         RETURN 1 /* nothing after '*' */
      p = LASTPOS(LEFT(tmp, 1), str)
      SAY 'p*: 'p
      IF p = 0 THEN
         RETURN 0 /* character after '*' not found */
      str = SUBSTR(str, p+1)
      tmp = SUBSTR(tmp, 2)
      END
   SIGNAL redo

/* replace parts of string
   StringReplace(string, find, replace [, find, replace[, ...]])
 */
StringReplace: PROCEDURE
   tmp = ARG(1)
   DO i = 2 BY 2
      IF ARG(i, 'O') THEN
         RETURN tmp
      p = POS(ARG(i), tmp)
      DO WHILE p \= 0
         tmp = INSERT(ARG(i+1), DELSTR(tmp, p, LENGTH(ARG(i))), p-1)
         p = POS(ARG(i), tmp, p + LENGTH(ARG(i+1)))
         END
      END

/* quote % characters */
PCTesc: PROCEDURE
   tmp = ARG(1)
   p = POS('%', tmp)
   DO WHILE p \= 0
      tmp = INSERT('%', tmp, p)
      p = POS('%', tmp, p+2)
      END
   RETURN tmp

DirSpec: PROCEDURE
   RETURN STRIP(FILESPEC('D', ARG(1))FILESPEC('P', ARG(1)), 'T', '\')

IsDir: PROCEDURE
   tmp = STRIP(TRANSLATE(ARG(1), '\', '/'), 'T', '\')
   RETURN FILESPEC('d',ARG(1)) = ARG(1) | (STREAM(tmp, 'c', 'query size') = 0 & STREAM(tmp, 'c', 'query exists') = '')


/****************************************************************************
   Sorting
*/

/* sort stem variable
   call qsort stem[, first][, last]
   stem
     stem variable name to sort
   first, last (optional)
     sort range tree.index.first to tree.index.last
   This function calls itself recursively.
*/
QSort:
   stem = TRANSLATE(ARG(1))
   IF ARG(2,'e') THEN DO
      IF ARG(3,'e') THEN
         CALL DoQSort ARG(2), ARG(3)
      ELSE
         CALL DoQSort ARG(2), VALUE(stem'.'0)
      END
   ELSE IF ARG(3,'e') THEN
      CALL DoQSort 1, ARG(3)
   ELSE    
      CALL DoQSort 1, VALUE(stem'.'0)
   RETURN

DoQSort: PROCEDURE EXPOSE stem (stem)
   lo = ARG(1)
   hi = ARG(2)
   mi = (hi + lo) %2
   m = VALUE(stem'.'mi)
   /*SAY lo hi mi m*/
   DO WHILE hi >= lo
      DO WHILE VALUE(stem'.'lo) < m
         lo = lo +1
         END
      DO WHILE VALUE(stem'.'hi) > m
         hi = hi -1
         END
      IF hi >= lo THEN DO
         tmp = VALUE(stem'.'lo)
         CALL VALUE stem'.'lo, VALUE(stem'.'hi)
         CALL VALUE stem'.'hi, tmp
         lo = lo +1
         hi = hi -1
         END
      END
   IF hi > ARG(1) THEN
      CALL DoQSort ARG(1), hi
   IF ARG(2) > lo THEN
      CALL DoQSort lo, ARG(2)
   RETURN


/****************************************************************************
   Error handling
*/
Error: PROCEDURE
   CALL LINEOUT STDERR, ARG(2)
   EXIT ARG(1)

Warning: PROCEDURE
   IF ARG(1) \= '' THEN
      CALL LINEOUT STDERR, ARG(1)
   RETURN ARG(1)

