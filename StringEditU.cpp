/*========================================================================*/
/*                                                                        */
/* StringEditU  = Hilfs-Unit  zum Editieren von Strings und Zahlen        */
/*                ohne viel(?!) Heck-Meck....                             */
/*                                                                        */
/*   REV:     0.1                                                         */
/*   DATE:    2002-02-13 (YYYY-MM-DD)                                     */
/*   AUTHOR:  W.Buescher                                                  */
/*                                                                        */
/*========================================================================*/

//---------------------------------------------------------------------------
#include <vcl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#pragma hdrstop

//#include "YHF_Help.h"   // DL4YHF's HTML-replacement for WinHelp

#include "StringEditU.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"

//---------------------------------------------------------------------------
TStringEditForm *StringEditForm;
int  StringEdit_iEditOptions;


//---------------------------------------------------------------------------
//   Some wrappers for the String Edit - Form ...
//---------------------------------------------------------------------------


/****************************************************************************/
int RunIntegerInputDialog(   // a MODAL dialog; return: 0=cancel 1=ok
  char *pszTitle, char *pszLabel1,  char *pszLabel2,
  int  *piValue,  int iEditOptions, int  iHelpContext )
{
  char sz80Temp[81];

   StringEdit_iEditOptions = iEditOptions;
   StringEditForm->Caption = pszTitle;
   if(pszLabel1)
           StringEditForm->Label1->Caption = pszLabel1;
     else  StringEditForm->Label1->Caption = "";
   if(pszLabel2)
           StringEditForm->Label2->Caption = pszLabel2;
     else  StringEditForm->Label2->Caption = "";
   if(iEditOptions & EDIT_HEXADECIMAL)
           sprintf(sz80Temp,"$%04X",*piValue);
     else  sprintf(sz80Temp, "%d",  *piValue);
   StringEditForm->Edit1->Text = sz80Temp;
   StringEditForm->m_iHelpContext = iHelpContext;
   if(StringEditForm->ShowModal() == mrOk)
    {
      strncpy(sz80Temp, StringEditForm->Edit1->Text.c_str(), 80);
      if(sz80Temp[0]=='$')
        { sscanf(sz80Temp+1,"%X", *piValue );  // caution: lousy error handling in some scanf implementations !
        }
      else
        { *piValue = StrToIntDef( StringEditForm->Edit1->Text, *piValue );
        }
     return 1;
    }
   else // user clicked "cancel" button:
    { return 0;
    }
} // end RunIntegerInputDialog()

/****************************************************************************/
int RunFloatInputDialog( char *pszTitle, char *pszLabel1, char *pszLabel2,
                         double *pdblValue,  int iEditOptions, int  iHelpContext )
{
  char sz80Temp[81];
  double dblTemp;

   StringEdit_iEditOptions = iEditOptions;
   StringEditForm->Caption = pszTitle;
   if(pszLabel1)
           StringEditForm->Label1->Caption = pszLabel1;
     else  StringEditForm->Label1->Caption = "";
   if(pszLabel2)
           StringEditForm->Label2->Caption = pszLabel2;
     else  StringEditForm->Label2->Caption = "";
   iEditOptions = iEditOptions;  // unused so far
   sprintf(sz80Temp,"%.3lf", *pdblValue);
   StringEditForm->Edit1->Text = sz80Temp;
   StringEditForm->m_iHelpContext = iHelpContext;
   if(StringEditForm->ShowModal() == mrOk)
    {
      strncpy(sz80Temp, StringEditForm->Edit1->Text.c_str(), 80);
      if(sz80Temp[0]>0)
       { dblTemp = strtod( sz80Temp, NULL);
         if( dblTemp!=HUGE_VAL && dblTemp!=-HUGE_VAL )
          { *pdblValue = dblTemp;
            return 1;
          }
       }
    }
   return 0; // user clicked "cancel" button, or conversion error in strtod()
} // end RunIntegerInputDialog()



/****************************************************************************/
int RunStringEditDialog( // returns a button code like mrOk (!)
                         char *pszTitle,
                         char *pszLabel1, char *pszLabel2,
                         char *pszDefaultValue, int iEditOptions,
                         char *pszDestination,  int iMaxLength,
                         int  iHelpContext )
{
 int iModalResult;

   StringEdit_iEditOptions = iEditOptions;
   StringEditForm->Caption = pszTitle;
   if(pszLabel1)
           StringEditForm->Label1->Caption = pszLabel1;
     else  StringEditForm->Label1->Caption = "";
   if(pszLabel2)
           StringEditForm->Label2->Caption = pszLabel2;
     else  StringEditForm->Label2->Caption = "";
   if(pszDefaultValue)
           StringEditForm->Edit1->Text = pszDefaultValue;
     else  StringEditForm->Edit1->Text = "";
   StringEditForm->m_iHelpContext = iHelpContext;
   iModalResult = StringEditForm->ShowModal();
   if(iModalResult == mrOk)
    {
      strncpy( pszDestination, StringEditForm->Edit1->Text.c_str() ,iMaxLength);
      pszDestination[iMaxLength] = 0;
    }
   return iModalResult;
} // end RunStringEditDialog()


//---------------------------------------------------------------------------
__fastcall TStringEditForm::TStringEditForm(TComponent* Owner)
        : TForm(Owner)
{
}
//---------------------------------------------------------------------------

void __fastcall TStringEditForm::Btn_OkClick(TObject *Sender)
{
   ModalResult = mrOk;
}
//---------------------------------------------------------------------------

void __fastcall TStringEditForm::FormActivate(TObject *Sender)
{
  Btn_Help->Enabled = (m_iHelpContext>=0);
  StringEditForm->Edit1->SetFocus();
  if(StringEdit_iEditOptions & EDIT_SELECT_ALL)
   { // select the whole text in a TEdit ..
     StringEditForm->Edit1->SelectAll();
   }
}
//---------------------------------------------------------------------------

void __fastcall TStringEditForm::Btn_HelpClick(TObject *Sender)
{
  if(m_iHelpContext>0)
    { // ex: Application->HelpContext( m_iHelpContext );
      // Caution: Refused to work when compiled with Borland C++ Builder V6.0 - so :
      //  YHF_HELP_ShowHelpContext(m_iHelpContext);
    }
}
//---------------------------------------------------------------------------

