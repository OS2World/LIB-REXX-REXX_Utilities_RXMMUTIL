CARG = -Zomf -Zmt -Zdll -Wall -Zcrtdll -mprobe -s -O6 -ffloat-store -fomit-frame-pointer -ffast-math -mcpu=pentium -funsigned-char
#CARG = -Zmt -Wall -Zdll -Zcrtdll -mprobe -O6 -ffloat-store -fomit-frame-pointer -ffast-math -mcpu=athlon -funsigned-char
# gcc 3.0
#LDARG = -lstdcxx -lunicode
# gcc 3.3
LDARG = -lstdc++

# gcc 3.0 fix, remove if not needed
#LDARG += $(EMX)\lib\gcc3fix.c

# comment the following lines if you don't have Wei Dai's cryptlib
CARG_CRYPTLIB = -DHAVE_CRYPTLIB -I../../sonstig/cryptlib
LDARG_CRYPTLIB = -llibcryptopp -L../../sonstig/cryptlib

CARG += $(CARG_CRYPTLIB)

rxmmutl.dll : rxmmutl.o rxmmutl.def
	gcc $(CARG) rxmmutl.o rxmmutl.def $(LDARG_CRYPTLIB) $(LDARG)
	lxlite rxmmutl.dll /C:DLL

rxmmutl.o : rxmmutl.cpp makefile
	gcc -c rxmmutl.cpp $(CARG)
