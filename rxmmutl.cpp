/****************************************************************************
*
*  rxmmutl.cpp
*
*  Create:  20.11.2001
*  Update:  02.12.2001
*  (C)      Marcel MÅller 2001
*
****************************************************************************/


#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>

#include <string>
#include <map>

using namespace std;

#ifdef HAVE_CRYPTLIB
#include <md2.h>
#include <md4.h>
#include <md5.h>
#include <crc.h>
#include <ripemd.h>
#include <sha.h>
#include <tiger.h>
#include <haval.h>
#include <adler32.h>
#include <panama.h>
#endif

extern "C"
{
   #define  INCL_WIN
   #define  INCL_ERRORS
   #define  INCL_BASE
   #define  INCL_REXXSAA
   #define  INCL_DOSMODULEMGR
   #include <os2.h>
   #define  VALID_ROUTINE   0
   #define  INVALID_ROUTINE 1
}


#define RXFUNCTIONDEF extern "C" ULONG APIENTRY

#define BUILDRXSTRING(t, s) memcpy((t)->strptr,(s),(t)->strlength = strlen((s)));

/* Convert REXX string to unsigned long
   return true  conversion succesful
          false error
*/
static bool str2ulong(const RXSTRING str, unsigned long* dst)
{  if (!RXVALIDSTRING(str))
      return false;
   PSZ cp = str.strptr;
   ULONG len = str.strlength;
   *dst = 0;
   for(;;)
   {  if (!isdigit(*cp))
         return false;
      *dst *= 10;
      if (*dst > ULONG_MAX - *cp + '0')
         return false;
      *dst += *cp++ - '0';
      if (!--len)
         return true;
      if (*dst > ULONG_MAX/10)
         return false;
}  }

/* prepare memory for a REXX string, allocate memory if the destination buffer is too small 
   return true  successful
          false out of memory
*/
static inline bool rxstrreserve(RXSTRING* dst, ULONG len)
{  if (len > dst->strlength && DosAllocMem((PPVOID)&dst->strptr, len, PAG_COMMIT|PAG_WRITE) != NO_ERROR)
      return false;
   dst->strlength = len;
   return true;
}

/* copy REXX string and allocate memory if the destination buffer is too small 
   return true  successful
          false out of memory
*/
static bool rxstrcpy(const RXSTRING src, RXSTRING* dst)
{  if (!rxstrreserve(dst, src.strlength))
   {  BUILDRXSTRING(dst, "Error:");
      return false;
   }
   memcpy(dst->strptr, src.strptr, src.strlength);
   return true;
}

/* critical section object */
struct CriticalSection
{  CriticalSection()				{ DosEnterCritSec(); }
   ~CriticalSection()			{ DosExitCritSec(); }
};

class Mutex
{public:
	friend class Lock
	{	Mutex& M;
	 public:
		Lock(Mutex& m)	: M(m)	{ DosRequestMutexSem(M.Handle, SEM_INDEFINITE_WAIT); }
		~Lock()						{ DosReleaseMutexSem(M.Handle); }
	};
 protected:
	HMTX	Handle;
 public:
	Mutex() : Handle(NULLHANDLE) { DosCreateMutexSem(NULL, &Handle, 0, FALSE); }
	~Mutex()							{ DosCloseMutexSem(Handle); }
};


/* Managed Ressource: HAB */

/* Get HAB for Win... functions */
static class myHAB
{  HAB hab;
   bool habd;
 public:
   myHAB() : hab(NULLHANDLE), habd(false) {}
   ~myHAB();
   operator HAB();
} Hab;

/* get HAB */
myHAB::operator HAB()
{  if (hab != (HAB)0)
      return hab;
   CriticalSection cs;
   habd = !(hab = WinInitialize(0));// create anchor block
   if (!habd)
      hab = WinQueryAnchorBlock(HWND_DESKTOP);// get desktop anchor
   return hab;
}

/* free HAB */
myHAB::~myHAB()
{  if (!habd && hab != (HAB)0)
      WinTerminate(hab);
   hab = NULLHANDLE;
}


/* Interface functions */

/* Translate string from source CP to destination CP
   ARG(1) string
   ARG(2) source CP, current CP by default
   ARG(3) destination CP, current CP by default
   ARG(4) substitution char, 0xFF by default, '' => leave unchanged
*/
RXFUNCTIONDEF MMTranslateCp(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  if (numargs < 1 || numargs > 4 || !args[0].strptr)     // check arguments
      return INVALID_ROUTINE;

   ULONG scp = 0, dcp = 0, dummy;
   UCHAR sub = 0xFF;
   bool nosub = false;
   // error char
   if (numargs >= 4 && args[3].strptr)
      if (args[3].strlength > 1)
         return INVALID_ROUTINE;
       else if (args[3].strlength)
         sub = *args[3].strptr;
       else
         nosub = true;
   // source CP
   if (numargs >= 2 && args[1].strptr && !str2ulong(args[1], &scp))
      return INVALID_ROUTINE;
   if (scp == 0)
      DosQueryCp(sizeof(ULONG), &scp, &dummy);// current cp
   // destination CP
   if (numargs >= 3 && args[2].strptr && !str2ulong(args[2], &dcp))
      return INVALID_ROUTINE;
   if (dcp == 0)
      DosQueryCp(sizeof(ULONG), &dcp, &dummy);// current cp
   // duplicate source string
   rxstrcpy(args[0], retstr);
   if (scp == dcp || retstr->strlength == 0)
      return VALID_ROUTINE;
   // translate string
   HAB hab = Hab;
   PSZ cp = retstr->strptr + retstr->strlength;
   do
   {  if (*--cp != 0 && *cp != 0xFF)
         if ((*cp = WinCpTranslateChar(hab, scp, *cp, dcp)) == 0)
         {  // Error
            BUILDRXSTRING(retstr, "Error:");
            break;
         } else if (*cp == 0xFF)
         {  if (nosub)
               *cp = args[0].strptr[cp - retstr->strptr];
             else
               *cp = sub;
         }
   } while (cp != retstr->strptr);
   return VALID_ROUTINE;
}

/* Translate string to uppercase using special CP
   ARG(1) string
   ARG(2) CP, current CP by default
   ARG(3) country code, from CONFIG.SYS by default
*/
RXFUNCTIONDEF MMUpper(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  if (numargs < 1 || numargs > 3 || !args[0].strptr)     // check arguments
      return INVALID_ROUTINE;

   ULONG scp = 0, cc = 0;
   // CP
   if (numargs >= 2 && args[1].strptr && !str2ulong(args[1], &scp))
      return INVALID_ROUTINE;
   // country code
   if (numargs >= 3 && args[2].strptr && !str2ulong(args[2], &cc))
      return INVALID_ROUTINE;
   // duplicate source string
   rxstrcpy(args[0], retstr);
   if (retstr->strlength == 0)
      return VALID_ROUTINE;
   // translate string
   HAB hab = Hab;
   PSZ cp = retstr->strptr + retstr->strlength;
   do
   {  if (*--cp != 0 && (*cp = WinUpperChar(hab, scp, cc, *cp)) == 0)
      {  // Error
         BUILDRXSTRING(retstr, "Error:");
         break;
      }
   } while (cp != retstr->strptr);
   return VALID_ROUTINE;
}

/* Get UNIX-Time
*/
RXFUNCTIONDEF MMTime(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  if (numargs > 0)                    // check arguments
      return INVALID_ROUTINE;
   retstr->strlength = sprintf((char*)retstr->strptr, "%lu", time(NULL));
   return VALID_ROUTINE;
}

/* Set file size
   ARG(1) filename
   ARG(2) new size
*/
RXFUNCTIONDEF MMSetFileSize(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  ULONG size;
   if (numargs != 2 || !args[0].strptr || !args[1].strptr || !str2ulong(args[1], &size)) // check arguments
      return INVALID_ROUTINE;
   HFILE f;
   ULONG d;
   APIRET r = DosOpen(args[0].strptr, &f, &d, 0, 0, OPEN_ACTION_FAIL_IF_NEW|OPEN_ACTION_OPEN_IF_EXISTS,
    OPEN_SHARE_DENYNONE|OPEN_ACCESS_WRITEONLY, NULL);
   if (r == NO_ERROR)
   {  r = DosSetFileSize(f, size);
      DosClose(f);
   }
   retstr->strlength = sprintf((char*)retstr->strptr, "%lu", r);
   return VALID_ROUTINE;
}

/* Move file or directory
   ARG(1) old filename
   ARG(2) new filename
*/
RXFUNCTIONDEF MMFileMove(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  if (numargs != 2 || !args[0].strptr || !args[1].strptr) // check arguments
      return INVALID_ROUTINE;
   APIRET r = DosMove(args[0].strptr, args[1].strptr);
   retstr->strlength = sprintf((char*)retstr->strptr, "%lu", r);
   return VALID_ROUTINE;
}

RXFUNCTIONDEF MMFileIn(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  if (numargs != 1 || !args[0].strptr)
      return INVALID_ROUTINE;
   HFILE hf = NULLHANDLE;
   ULONG dummy;
   APIRET r;
   FILESTATUS3 fst;
   fst.cbFile = 0;
   do
   {  r = DosOpen(args[0].strptr, &hf, &dummy, 0, 0, OPEN_ACTION_FAIL_IF_NEW|OPEN_ACTION_OPEN_IF_EXISTS,
       OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYWRITE|OPEN_ACCESS_READONLY, NULL);
      if (r != NO_ERROR)
         break;
      r = DosQueryFileInfo(hf, FIL_STANDARD, &fst, sizeof fst);
      if (r != NO_ERROR)
         break;
      if (fst.cbFile > 256)
      {  r = DosAllocMem(&(void*&)retstr->strptr, fst.cbFile, PAG_COMMIT|PAG_READ|PAG_WRITE);
         if (r != NO_ERROR)
            break;
      }
      r = DosRead(hf, retstr->strptr, fst.cbFile, &dummy);
      if (r != NO_ERROR)
         break;
      if (dummy < fst.cbFile)
      {  r = (APIRET)-1; /* well, normally this should never happen ... */
         break;
      }
      // return success
      retstr->strlength = fst.cbFile;
   } while (0);
   // close
   if (hf != NULLHANDLE)
      DosClose(hf);
   if (r != 0)
   {  // error
      if (fst.cbFile > 256)
         DosFreeMem(retstr->strptr);
      retstr->strlength = 0;
      //retstr->strlength = sprintf((char*)retstr->strptr, "%lu", r);
   }
   return VALID_ROUTINE;
}

/* Calculate hash value
   ARG(1) input string
   ARG(2) hash type
   return hashvalue or '' on error
*/
#ifdef HAVE_CRYPTLIB
using namespace CryptoPP;

template <class T>
static HashModule* DefalutHashFactory(ULONG numargs, RXSTRING*)
{  return numargs ? NULL : new T();
}

struct HashEntry
{  const char* name;
   HashModule* (*factory)(ULONG numargs, RXSTRING* args);
} const HashList[] =
#define DEFHE(name) {#name, &DefalutHashFactory<name>}
// the following list MUST be in case insensitive order
{  DEFHE(Adler32),
   DEFHE(CRC32),
   DEFHE(HAVAL),
   DEFHE(MD2),
   DEFHE(MD4),
   DEFHE(MD5),
   DEFHE(RIPEMD160),
   DEFHE(SHA),
   DEFHE(SHA256),
   DEFHE(SHA384),
   DEFHE(SHA512),
   DEFHE(Tiger),
};
#undef DEFHE

static int HashEntryCmp(const char* key, const HashEntry* elem)
{  return stricmp(key, elem->name);
}
#endif

RXFUNCTIONDEF MMHash(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  if (numargs < 2 || !args[0].strptr || !args[1].strptr)
      return INVALID_ROUTINE;
   #ifdef HAVE_CRYPTLIB
   const HashEntry* he;
   if ( args[1].strlength > 15
       || (args[1].strptr[args[1].strlength] = 0, // assuming at least 16 bytes of storage for the input buffer
      // hash factory
        he = (HashEntry*)bsearch(args[1].strptr, HashList, sizeof HashList / sizeof *HashList, sizeof *HashList,
         (int (*)(const void*, const void*))HashEntryCmp)) == NULL )
      // hash type not found
      return INVALID_ROUTINE;
   HashModule* hm = he->factory(numargs-2, args+2);
   if (hm)
   {  // calc hash
      hm->CalculateDigest(retstr->strptr, args[0].strptr, args[0].strlength); // assuming digestlength < 256
      retstr->strlength = hm->DigestSize();
      delete hm;
      return VALID_ROUTINE;
   }
   #endif
   // some error occured
   retstr->strlength = 0;
   return VALID_ROUTINE;
}


static class myOpenInis
{protected:
   typedef map<string, HINI> IndexType;
   IndexType   Index;
	Mutex		   CS;
 public:
	HINI        Open(const string& name);
   ERRORID     Close(const string& name);
   ~myOpenInis();
} OpenInis;

HINI myOpenInis::Open(const string& name)
{  Mutex::Lock cs(CS);
   // already open
   IndexType::const_iterator i = Index.find(name);
   if (i != Index.end())
   {  //printf("myOpenInis(%p)::Open(%s) -> %lx\n", this, name.c_str(), i->second);
      return i->second;
   }
   // open
   HINI h = PrfOpenProfile(Hab, (const UCHAR*)name.c_str());
   //printf("myOpenInis(%p)::Open(%s) -> new %lx\n", this, name.c_str(), h);
   if (h != NULLHANDLE)
      Index.insert(IndexType::value_type(name, h));
   return h;
}

ERRORID myOpenInis::Close(const string& name)
{	Mutex::Lock cs(CS);
   IndexType::iterator i = Index.find(name);
   if (i == Index.end())
 	{  //printf("myOpenInis(%p)::Close(%s) -> unknown\n", this, name.c_str());
 		return PMERR_INVALID_INI_FILE_HANDLE;
   }
   //printf("myOpenInis(%p)::Close(%s) -> %lx\n", this, name.c_str(), i->second);
 	if (!PrfCloseProfile(i->second))
 		return WinGetLastError(Hab);
 	Index.erase(i);
 	return 0;
}

myOpenInis::~myOpenInis()
{  for (IndexType::const_iterator i = Index.begin(); i != Index.end(); ++i)
   {  //DosBeep(500, 500);
      //printf("myOpenInis(%p)::~myOpenInis(): closing %lx.\n", this, i->second);
      PrfCloseProfile(i->second);
   }
   Index.clear();
}

RXFUNCTIONDEF MMIniOpen(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  if (numargs != 1 || !args[0].strptr)
      return INVALID_ROUTINE;
   HINI h = OpenInis.Open((const char*)args[0].strptr);
   retstr->strlength = sprintf((char*)retstr->strptr, "%lu", h == NULLHANDLE ? WinGetLastError(Hab) : 0UL);
   return VALID_ROUTINE;
}

RXFUNCTIONDEF MMIniClose(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  if (numargs != 1 || !args[0].strptr)
      return INVALID_ROUTINE;
   retstr->strlength = sprintf((char*)retstr->strptr, "%lu", OpenInis.Close((const char*)args[0].strptr));
   return VALID_ROUTINE;
}

RXFUNCTIONDEF MMIniQuery(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  if (numargs < 1 || numargs > 3 || !args[0].strptr)
      return INVALID_ROUTINE;
   const UCHAR* app = numargs >= 2 ? args[1].strptr : NULL;
   const UCHAR* key = numargs >= 3 ? args[2].strptr : NULL;
   if (app == NULL && key != NULL)
      return INVALID_ROUTINE;
   HINI h = OpenInis.Open((const char*)args[0].strptr);
   if (h != NULLHANDLE)
   {  ULONG len;
      if ( PrfQueryProfileSize(h, app, key, &len)
         && rxstrreserve(retstr, len)
         && PrfQueryProfileData(h, app, key, retstr->strptr, &len) )
      {  if (key == NULL && len > 0)
            --len;
         retstr->strlength = len;
         return VALID_ROUTINE;
   }  }
   // some error occured
   retstr->strlength = 0;
   return VALID_ROUTINE;
}

RXFUNCTIONDEF MMIniWrite(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  if (numargs < 2 || numargs > 4 || !args[0].strptr || !args[1].strptr)
      return INVALID_ROUTINE;
   const UCHAR* key = numargs >= 3 ? args[2].strptr : NULL;
   const UCHAR* val = numargs >= 4 ? args[3].strptr : NULL;
   if (key == NULL && val != NULL)
      return INVALID_ROUTINE;
   HINI h = OpenInis.Open((const char*)args[0].strptr);
   if ( h != NULLHANDLE
      && PrfWriteProfileData(h, args[1].strptr, key, val, args[3].strlength) )
   {  BUILDRXSTRING(retstr, "0");
      return VALID_ROUTINE;
   }
   // some error occured
   retstr->strlength = sprintf((char*)retstr->strptr, "%lu", WinGetLastError(Hab));
   return VALID_ROUTINE;
}


/********** Load all functions */
static PSZ RxFncTable[] =
{  (PSZ)"MMTranslateCp",
   (PSZ)"MMUpper",
   (PSZ)"MMTime",
   (PSZ)"MMSetFileSize",
   (PSZ)"MMFileMove",
   (PSZ)"MMFileIn",
   (PSZ)"MMHash",
   (PSZ)"MMIniOpen",
   (PSZ)"MMIniClose",
   (PSZ)"MMIniQuery",
   (PSZ)"MMIniWrite",
   (PSZ)"MMDropFuncs",
   (PSZ)"MMLoadFuncs"
};
RXFUNCTIONDEF MMLoadFuncs(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  retstr->strlength = 0;              // set return value
   if (numargs > 0)                    // check arguments
      return INVALID_ROUTINE;

   const PSZ* fp = RxFncTable + sizeof(RxFncTable) / sizeof(PSZ) -1;
   do
   {  --fp;
      RexxRegisterFunctionDll(*fp, (PSZ)"RXMMUTL", *fp);
      /*ULONG ul = RexxRegisterFunctionDll(*fp, (PSZ)"RXMMUTL", *fp);
      if (ul != 0)
         printf("RexxRegisterFunctionDll(%s,...) -> %lu\n", *fp, ul);*/
   } while (fp != RxFncTable);
   return VALID_ROUTINE;
}

RXFUNCTIONDEF MMDropFuncs(const UCHAR *name, ULONG numargs, RXSTRING args[], const UCHAR *queuename, RXSTRING *retstr)
{  retstr->strlength = 0;              // set return value
   if (numargs > 0)                    // check arguments
      return INVALID_ROUTINE;

   const PSZ* fp = RxFncTable + sizeof(RxFncTable) / sizeof(PSZ);
   do RexxDeregisterFunction(*--fp);
    while (fp != RxFncTable);
   return VALID_ROUTINE;
}

