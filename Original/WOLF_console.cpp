//---------------------------------------------------------------------------
//  DL4YHF's very first attempt to compile and run Stewart Nelson's WOLF code
//  under Borland C++ Builder,
//  with separation between USER INTERFACE and WOLF DSP CODE .
//---------------------------------------------------------------------------


#pragma hdrstop
#include <condefs.h>


//---------------------------------------------------------------------------
USEUNIT("wolf.cpp");
USEUNIT("fft.cpp");
USEUNIT("wolfio_console.cpp");
//---------------------------------------------------------------------------
#pragma argsused

extern int WOLF_Main(int argc, char* argv[]);

int main(int argc, char* argv[])
{
  return WOLF_Main(argc, argv);
}

