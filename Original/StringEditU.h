/*========================================================================*/
/*                                                                        */
/* StringEditU  = Hilfs-Unit  zum Editieren von Strings und Zahlen        */
/*                ohne viel Heck-Meck....                                 */
/*                                                                        */
/*   REV:     0.1                                                         */
/*   DATE:    13.02.2002                                                  */
/*   AUTHOR:  W.Buescher                                                  */
/*                                                                        */
/*========================================================================*/

//---------------------------------------------------------------------------
#ifndef StringEditUH
#define StringEditUH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
//---------------------------------------------------------------------------


   // options for the parameter 'int iEditOptions' ....
#define EDIT_NORMAL      0
#define EDIT_SELECT_ALL  1
#define EDIT_HEXADECIMAL 2




//---------------------------------------------------------------------------
class TStringEditForm : public TForm
{
__published:	// Von der IDE verwaltete Komponenten
        TLabel *Label1;
        TLabel *Label2;
        TBitBtn *Btn_Cancel;
        TBitBtn *Btn_Ok;
        TEdit *Edit1;
        TBitBtn *Btn_Help;
        void __fastcall Btn_OkClick(TObject *Sender);
        void __fastcall FormActivate(TObject *Sender);
        void __fastcall Btn_HelpClick(TObject *Sender);
private:	// Anwenderdeklarationen
public:		// Anwenderdeklarationen
        int m_iHelpContext;
        __fastcall TStringEditForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TStringEditForm *StringEditForm;
//---------------------------------------------------------------------------




// Outside the input dialog class ...


/****************************************************************************/
int RunIntegerInputDialog( char *pszTitle, char *pszLabel1, char *pszLabel2,
                         int  *piValue,  int iEditOptions, int  iHelpContext );

/****************************************************************************/
int RunFloatInputDialog( char *pszTitle, char *pszLabel1, char *pszLabel2,
                         double *pdblValue,  int iEditOptions, int  iHelpContext );


/****************************************************************************/
int RunStringEditDialog( char *pszTitle, char *pszLabel1, char *pszLabel2,
                         char *pszDefaultValue, int iEditOptions,
                         char *pszDestination,  int iMaxLength ,
                         int  iHelpContext );




#endif
