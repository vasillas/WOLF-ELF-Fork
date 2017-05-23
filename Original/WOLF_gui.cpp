//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
USEFORM("WOLF_GUI1.cpp", WolfMainForm);
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
