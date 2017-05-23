//-----------------------------------------------------------------------
// AAMachine.h: Header for the Auto-Answering Machine for the WOLF GUI
//             ( Weak-signal Operation on Low Frequency,
//               WOLF-core written by Stewart Nelson, KK7KA )
//
// Author  : Wolfgang Buescher (DL4YHF) 04/2005
// Location: <dl4yhf>C:\CBproj\WOLF\AAMachine.h
// Compiler: Borland C++Builder V4 (but no Borland-specific stuff in here)
//
//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
//  Defines
//-----------------------------------------------------------------------

#define AAM_MAX_MESSAGES 16  // max number of message definition lines

   // possible values for T_AAM_DefinitionLine.iDir (RX/TX direction)
#define AAM_DIR_UNUSED   0
#define AAM_DIR_RX       1
#define AAM_DIR_TX       2

   // possible values for T_AAM_DefinitionLine.iFlags (special flags)
#define AAM_FLAGS_NORMAL 0


//-----------------------------------------------------------------------
//  Data types
//-----------------------------------------------------------------------

typedef struct // T_AAM_DefinitionLine
{
  char sz80RxMsg[84];
  char sz80TxMsg[84];
  char sz80Action[84];
} T_AAM_DefinitionLine;


typedef struct // T_AAMachine
{
  char sz16MyCall[20];
  char sz16DxCall[20];

  T_AAM_DefinitionLine defs[AAM_MAX_MESSAGES];

  char  sz80NextTxMsg[84];

  int   iTxOversLeft;  // count of TX-periods left (0=stop)

} T_AAMachine;





//-----------------------------------------------------------------------
//  Global vars
//-----------------------------------------------------------------------

  // none !


//-----------------------------------------------------------------------
//  Prototypes
//-----------------------------------------------------------------------

void  AAM_Init( T_AAMachine *pAAM );
int   AAM_HandleReceivedText( T_AAMachine *pAAM, char *pszRcvdMessage );
char *AAM_GetMacroText( T_AAMachine *pAAM, char *pszMacroName, int *piMacroNameLength );
void  AAM_SetMacroText( T_AAMachine *pAAM, char *pszMacroName, char *pszMacroText,
                        int *piMacroNameLength, int *piMacroTextLength );
int   AAM_ExpandMacroText( T_AAMachine *pAAM,
                        char *pszSource, char *pszDest, int iMaxDestLen );
char *AAM_GetTxText( T_AAMachine *pAAM );
char *AAM_GetTxText( T_AAMachine *pAAM );
void  AAM_SetTxText( T_AAMachine *pAAM, char *pszNewTxText );




// EOF < AAMachine.h >
