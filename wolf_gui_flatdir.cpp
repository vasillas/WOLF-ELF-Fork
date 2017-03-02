//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
USERES("WOLF_gui.res");
USEFORM("WOLF_GUI1.cpp", WolfMainForm);
USEUNIT("wolf.cpp");
USEUNIT("fft.cpp");
USEUNIT("WolfThd.cpp");
USEFILE("SWITCHES.H");
USEUNIT("LinradColours.C");
USEUNIT("Sound.cpp");
USEUNIT("ErrorCodes.cpp");
USEUNIT("SoundMaths.c");
USERES("MyOwnRes.res");
USEUNIT("AAMachine.cpp");
USEFORM("StringEditU.cpp", StringEditForm);
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
        try
        {
                 Application->Initialize();
                 Application->CreateForm(__classid(TWolfMainForm), &WolfMainForm);
                 Application->CreateForm(__classid(TStringEditForm), &StringEditForm);
                 Application->Run();
        }
        catch (Exception &exception)
        {
                 Application->ShowException(&exception);
        }
        return 0;
}
//---------------------------------------------------------------------------
