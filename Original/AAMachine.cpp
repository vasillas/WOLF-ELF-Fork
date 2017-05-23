//-----------------------------------------------------------------------
// AAMachine.cpp: Auto-Answering Machine for the WOLF GUI
//             ( Weak-signal Operation on Low Frequency,
//               WOLF-core written by Stewart Nelson, KK7KA )
//
// Author  : Wolfgang Buescher (DL4YHF) 04/2005
// Location: <dl4yhf>C:\CBproj\WOLF\AAMachine.cpp
// Compiler: Borland C++Builder V4 (but no Borland-specific stuff in here)
//
//-----------------------------------------------------------------------

#include <string.h>

#include "AAMachine.h"

//-----------------------------------------------------------------------
void AAM_Init( T_AAMachine *pAAM )
  // Initializes an Automatic Answering Machine .
  // All member variables are cleared with their "default" values .
{
  memset( pAAM, 0, sizeof(T_AAMachine) );
  pAAM->iTxOversLeft = 100;    // may transmit 100 times before "QRT"

} // end AAM_Init()

//-----------------------------------------------------------------------
int AAM_IsLetter( char c )
{ return (c>='a' && c<='z') || (c>='A' && c<='Z');
}

//-----------------------------------------------------------------------
char AAM_ToUpper( char c )
{ if(c>='a' && c<='z')
       return (char)(c-'a'+'A');  // without the cast: "conversion may lose blabla" ?!
  else return c;
}

//-----------------------------------------------------------------------
int AAM_ParseInteger( char **ppsz )
{ int iResult = 0;
  int iSign = 1;
  char *cp = *ppsz;
  while(*cp==' ') ++cp;
  if( *cp=='-' )       { ++cp;  iSign=-1;   }
  else if( *cp=='+' )  { ++cp;  iSign= 1;   }
  while(*cp>='0' && *cp<='9')
   { iResult = 10*iResult + (int)( (*cp++)-'0');
   }
  *ppsz = cp;
  return iResult * iSign;
} // end AAM_ParseInteger()

//-----------------------------------------------------------------------
int AAM_ExecuteCmdLine( T_AAMachine *pAAM, char *pszCmdLine )
{
  int iOk = 1;
  while(*pszCmdLine && iOk)
   {
    if( strncmp(pszCmdLine,"tc=",3)==0)
     { pszCmdLine += 3;
       pAAM->iTxOversLeft = AAM_ParseInteger( &pszCmdLine );
     }
    else // none of the known commands... end loop !
     { iOk=0; break;
     }
   }
  return iOk;
} // end AAM_ExecuteCmdLine()

//-----------------------------------------------------------------------
int AAM_HandleReceivedText( T_AAMachine *pAAM, char *pszRcvdMessage )
  // Handles a received message:
  // - compares the message text with all lines
  //   defined in the "Auto-RX/TX" - table
  // - updates some components in the AAM-control struct
  // Return value: number of matching lines found ,
  //   0 = no match found (couldn't handle the message) .
{
  int iMsgNr;
  int match, handled;
  int iNrMatches = 0;
  int iMacroNameLen, iMacroTextLen;
  char *pszRcvd;
  char *pszComp;
  char *pszMacro;
  char sz80Temp[84];
  char c1, c2;
  // int  iRcvdLen = strlen(pszRcvdMessage);

  for( iMsgNr=0; iMsgNr<AAM_MAX_MESSAGES; ++iMsgNr)
   {
     // Check for matching pattern in the received string :
     match = 1;
     pszComp = pAAM->defs[iMsgNr].sz80RxMsg;
     pszRcvd = pszRcvdMessage;
     pszMacro= NULL;
     do
      { c1 = *pszRcvd;
        c2 = *pszComp;
        if( c1==0 || c2==0 )
            break;
        handled = 0;
        if( c2>='a' && c2<='z')
         { // reference string contains a 'macro' like "mycall" or "dxcall" :
          pszMacro = AAM_GetMacroText( pAAM, pszComp, &iMacroNameLen );
          iMacroTextLen = 0;
          if(iMacroNameLen>0)  // valid macro name -> DEFINE IT or COMPARE IT ?
           {
             if( pszMacro[0]>32 )
              {   // macro already defined -> COMPARE it (like "DF0WD" instead of "mycall")
                iMacroTextLen = strlen( pszMacro );
                match &= ( strnicmp( pszMacro, pszRcvd, iMacroTextLen ) == 0 );
              }
             else // macro not defined yet -> DEFINE it (copy the text)
              {
               if( strncmp(pszComp,"dxcall",6) == 0 )
                { AAM_SetMacroText( pAAM, pszComp/*macro name*/, pszRcvd/*text*/,
                                    &iMacroNameLen, &iMacroTextLen );
                }
              }
             pszComp += iMacroNameLen; // skip the macro name
             pszRcvd += iMacroTextLen;
             handled = 1;
           } // end if < macro name recognized >
          else // < macro name not recognized.. >
           { handled = 0;
           }
         } // end if < macro name in lower case >
        if( ! handled )
         { // if not "handled" as macro, compare letters character by character:
          match &= ( AAM_ToUpper(c1) == AAM_ToUpper(c2) );
          if(*pszRcvd)
            ++pszRcvd;
          if(*pszComp)
            ++pszComp;
          handled = 1;
         }
      }while(  match && c1!=0 && c2!=0 && (*pszRcvd!=0)
             && (pszRcvd<(pszRcvdMessage+16)) );
     if( match && (*pszRcvd<=32) && (*pszComp<=32) )
      { // received string matches the pattern in this definition line :
        ++iNrMatches;
        if( pAAM->defs[iMsgNr].sz80TxMsg[0] > 32 )  // set new TX-message ?
         {
           if( AAM_ExpandMacroText( pAAM, pAAM->defs[iMsgNr].sz80TxMsg, sz80Temp, 80) )
            { // only use the new TX string if *all* macros have been expanded successfully !
              strncpy( pAAM->sz80NextTxMsg, sz80Temp, 80 );
            }
         }
        if( pAAM->defs[iMsgNr].sz80Action[0] > 32 )  // execute programmed "action" ?
         { AAM_ExecuteCmdLine( pAAM, pAAM->defs[iMsgNr].sz80Action );
         }

      }
   } // end for < all defined messages >

  return (iNrMatches > 0);
} // end AAM_HandleReceivedText()

//-----------------------------------------------------------------------
char *AAM_GetMacroText( T_AAMachine *pAAM, char *pszMacroName, int *piMacroNameLength )
{
  int  i;
  int  iMacroNameLength = 0;
  char *pszResult = "";

  if(strncmp(pszMacroName,"mycall",6)==0)
   { iMacroNameLength = 6;
     pszResult = pAAM->sz16MyCall;
   }
  else if(strncmp(pszMacroName,"dxcall",6)==0)
   { iMacroNameLength = 6;
     pszResult = pAAM->sz16DxCall;
   }
  else if(strncmp(pszMacroName,"mc",2)==0)   // returns "my" 2-letter SUFFIX
   { iMacroNameLength = 2;
     i  = strlen(pAAM->sz16MyCall);
     if( i>=2 ) pszResult = pAAM->sz16MyCall + i - 2;
     else       pszResult = pAAM->sz16MyCall;
   }
  else if(strncmp(pszMacroName,"dc",2)==0)   // returns "his" 2-letter SUFFIX
   { iMacroNameLength = 2;
     i  = strlen(pAAM->sz16DxCall);
     if( i>=2 ) pszResult = pAAM->sz16DxCall + i - 2;
     else       pszResult = pAAM->sz16DxCall;
   }
  if( piMacroNameLength )
     *piMacroNameLength = iMacroNameLength;
  return pszResult;
} // end AAM_GetMacroText()

//-----------------------------------------------------------------------
void  AAM_SetMacroText( T_AAMachine *pAAM,
                        char * pszMacroName, char * pszMacroText,
                        int *piMacroNameLength, int *piMacroTextLength )
  // Note: pszMacroName and pszMacroText may be terminated by SPACE characters!
{
  int i;
  int iMacroNameLength=0, iMacroTextLength=0;
  if(strncmp(pszMacroName,"mycall",6)==0)
   { iMacroNameLength = 6;
     for(i=0; i<16 && pszMacroText[i]>32; ++i)
      { pAAM->sz16MyCall[i] = pszMacroText[i];
        ++iMacroTextLength;
      }
     pAAM->sz16MyCall[i] = '\0';
   }
  if(strncmp(pszMacroName,"dxcall",6)==0)
   { iMacroNameLength = 6;
     for(i=0; i<16 && pszMacroText[i]>32; ++i)
      { pAAM->sz16DxCall[i] = pszMacroText[i];
        ++iMacroTextLength;
      }
     pAAM->sz16DxCall[i] = '\0';
   }
  // Note: "mc" and "dc" (mycall's short form and dxcall's short form)
  //       cannot be set here !


  if( piMacroNameLength )
     *piMacroNameLength = iMacroNameLength;
  if( piMacroTextLength )
     *piMacroTextLength = iMacroTextLength;

} // end AAM_SetMacroText()

//-----------------------------------------------------------------------
int AAM_ExpandMacroText( T_AAMachine *pAAM,
                 char * pszSource, char * pszDest, int iMaxDestLen )
{
  int iMacroNameLen, iMacroTextLen;
  int fOk = 1;
  char *pszMacro;
  char c;

  do
   { c = * pszSource;
     if( c==0 )
         break;
     if( c>='a' && c<='z')
      { // reference string contains a 'macro' like "mycall" or "dxcall" :
        pszMacro = AAM_GetMacroText( pAAM, pszSource, &iMacroNameLen );
        if(iMacroNameLen>0)  // valid macro name -> is it defined ?
         {
          if( pszMacro[0]>32 )
           { // macro is defined -> COPY it (like "DF0WD" instead of "mycall")
             iMacroTextLen = strlen( pszMacro );
             while( iMacroTextLen>0 && iMaxDestLen>0 )
              { *pszDest++ = *pszMacro++;
                --iMaxDestLen; --iMacroTextLen;
              }
           }
          else // there is a macro, but the text is empty -> ??
           { fOk = 0;  // avoid garbage (for example, if "mycall" undefined)
             while( iMacroNameLen>0 && iMaxDestLen>0 )  // ... but copy the string
              { *pszDest++ = *pszSource++;
                --iMaxDestLen; --iMacroNameLen;
              }
           }
          pszSource += iMacroNameLen; // skip the macro name
         } // end if < macro name recognized >
      } // end if < macro name in lower case >
     else
      { // if not "handled" as macro, copy letter unchanged :
        *pszDest++ = *pszSource++;
        --iMaxDestLen;
      }
   }while(  iMaxDestLen>0 && c!=0 );
  *pszDest = 0;
  return fOk;
} // end AAM_ExpandMacroText()


//-----------------------------------------------------------------------
char *AAM_GetTxText( T_AAMachine *pAAM )
{
  return pAAM->sz80NextTxMsg;
}

//-----------------------------------------------------------------------
void AAM_SetTxText( T_AAMachine *pAAM, char *pszNewTxText )
{
  strncpy( pAAM->sz80NextTxMsg, pszNewTxText, 79 );
  pAAM->sz80NextTxMsg[80] = 0;
}


// EOF < AAMachine.cpp >
