//-----------------------------------------------------------------------
// WOLF_GUI1.h: Header file for the WOLF GUI
//             ( Weak-signal Operation on Low Frequency,
//               WOLF-core written by Stewart Nelson, KK7KA )
//
// Author  : Wolfgang Buescher (DL4YHF) 04/2005
// Location: <dl4yhf>C:\CBproj\WOLF\WOLF_GUI1.h
//
//-----------------------------------------------------------------------

//---------------------------------------------------------------------------
#ifndef WOLF_GUI1H
#define WOLF_GUI1H
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Menus.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>
#include <Dialogs.hpp>
#include <CheckLst.hpp>
#include <Grids.hpp>
#include <Buttons.hpp>

//-------------------------------------------------------------------------
// non-VCL types
//-------------------------------------------------------------------------

typedef struct // T_ScaleParams
{ int iLeft, iTop, iRight, iBottom; // drawing area for the scale
  int iScaleLengthPixels;
  double dblValMin, dblValMax;      // displayed 'physical' value range
  int iPosition;  //  POS_TOP , POS_BOTTOM , POS_LEFT or POS_RIGHT
} T_ScaleParams;


//-------------------------------------------------------------------------
// declaration of the main form (a Borland-specific VCL object ! )
//-------------------------------------------------------------------------

//---------------------------------------------------------------------------
class TWolfMainForm : public TForm
{
__published:	// IDE-managed Components
        TMainMenu *MainMenu1;
        TMenuItem *MI_File;
        TMenuItem *MI_DecodeWAV;
        TMenuItem *MI_EncodeWAV;
        TMenuItem *N1;
        TMenuItem *MI_Quit;
        TMenuItem *MI_Help;
        TMenuItem *MI_About;
        TPageControl *PageControl1;
        TTabSheet *TS_MainScreen;
        TTabSheet *TS_WolfConfig;
        TTabSheet *TS_IO_Config;
        TScrollBox *SCB_Tx;
        TScrollBox *SCB_Rx;
        TScrollBox *SCB_Status;
        TSplitter *Splitter1;
        TLabel *Label1;
        TEdit *Ed_TxString;
        TLabel *Label3;
        TButton *Btn_ClearDecoderOutput;
        TRichEdit *REd_Decoder;
        TOpenDialog *OpenDialog1;
        TSaveDialog *SaveDialog1;
        TGroupBox *Grp_TxOptions;
        TLabel *Label12;
        TEdit *Ed_TxPhaseRevTime;
        TLabel *Label13;
        TEdit *Ed_OutputAttenuation;
        TTabSheet *TS_Specials;
        TLabel *Label14;
        TCheckListBox *CLB_SpecialOpts;
        TGroupBox *Grp_RxOpts;
        TLabel *Label15;
        TEdit *Ed_SlewRate;
        TTimer *Timer1;
        TLabel *Lab_Status;
        TCheckBox *CHK_VerboseMsgs;
        TLabel *Label16;
        TEdit *Ed_FreqToler;
        TLabel *Label17;
        TMenuItem *MI_Mode;
        TMenuItem *MI_QsoMode;
        TMenuItem *MI_Receive;
        TMenuItem *MI_Stop;
        TPanel *Pnl_RxTxOff;
        TLabel *Lab_RxTxInfo2;
        TLabel *Lab_RxTxInfo1;
        TShape *Shp_RxTxLed;
        TMenuItem *MI_Options;
        TMenuItem *MI_RXVolCtrl;
        TMenuItem *MI_TXVolCtrl;
        TLabel *Lab_StatusRight;
        TImage *Img_Tuning;
        TImage *Img_InputLevel;
        TMenuItem *MI_TuningScope;
        TMenuItem *MI_TScope_Waterfall;
        TMenuItem *MI_TScope_DecoderVars;
        TMenuItem *MI_TScope_Off;
        TMenuItem *MI_TScope_SpecGraph;
        TMenuItem *N2;
        TMenuItem *MI_SendTuneTone;
        TMenuItem *MI_RxFreqCheck;
        TLabel *Label2;
        TEdit *Ed_FreqMeasIntv;
        TCheckBox *CHK_RestartDecoder;
        TMenuItem *MI_WOLFsettings;
        TButton *Btn_CopyDecoderOutput;
        TLabel *Lab_CursorInfo1;
        TLabel *Lab_CursorInfo2;
        TTabSheet *TS_AAMachine;
        TStringGrid *SG_RxTxMsgs;
        TScrollBox *ScrollBox1;
        TButton *Btn_RestartDecoder;
        TMenuItem *MI_ScopeRes;
        TMenuItem *MI_ScopeResLow;
        TMenuItem *MI_ScopeResMed;
        TMenuItem *MI_ScopeResHigh;
        TMenuItem *N3;
        TMenuItem *MI_ScopeAmplRange;
        TLabel *Label26;
        TLabel *Label27;
        TEdit *Ed_NrRxFrames;
        TUpDown *UD_NrRxFrames;
        TEdit *Ed_TxSampleRate;
        TLabel *Label6;
        TEdit *Ed_RxSampleRate;
        TLabel *Label7;
        TLabel *Label4;
        TEdit *Ed_TxFcenter;
        TLabel *Label10;
        TLabel *Label5;
        TLabel *Label11;
        TEdit *Ed_RxFcenter;
        TLabel *Label9;
        TLabel *Label28;
        TGroupBox *Grp_AudioFiles;
        TRadioButton *RB_8BitWave;
        TRadioButton *RB_16BitWave;
        TLabel *Label8;
        TMenuItem *MI_TxBeacon;
        TMenuItem *N4;
        TMenuItem *MI_Manual;
        TMenuItem *N5;
        TCheckBox *Chk_AAMachineEnabled;
        TLabel *Label21;
        TEdit *Ed_MyCall;
        TLabel *Label22;
        TEdit *Ed_DxCall;
        TLabel *Label23;
        TComboBox *CB_ComPortNr;
        TLabel *Label24;
        TCheckBox *Chk_TestPTT;
        TCheckBox *Chk_TestAlarmBell;
        TMenuItem *MI_WfColorPal;
        TMenuItem *MI_CP_Linrad;
        TMenuItem *MI_CP_Sunrise;
        TMenuItem *MI_QSOModeParams;
        TGroupBox *GRP_QSOMode;
        TLabel *Label18;
        TEdit *Ed_PeriodMins;
        TLabel *Label19;
        TLabel *Label20;
        TRadioButton *RB_TxFirst;
        TRadioButton *RB_TxLast;
        TCheckBox *CHK_ShowTimeOfDay;
        TCheckBox *CHK_AlarmOnReception;
        TEdit *Ed_TxOversLeft;
        TLabel *Label25;
        TButton *Btn_CalRxSr;
        TLabel *Label29;
        TLabel *Label30;
        TLabel *Lab_PkInfo1;
        TLabel *Lab_PkInfo2;
        TMenuItem *MI_WolfSpeed;
        TMenuItem *MI_NormalSpeed;
        TMenuItem *MI_HalfSpeed;
        TMenuItem *MI_QuarterSpeed;
        TMenuItem *MI_DoubleSpeed;
        TMenuItem *N6;
        TMenuItem *MI_FourfoldSpeed;
        TCheckBox *Chk_AddNoiseOnRx;
        TEdit *Ed_AddedNoiseLevel;
        TLabel *Label31;
        TMenuItem *N7;
        TMenuItem *MI_UsePrefilter;
   TLabel *Label32;
   TComboBox *CB_SoundInputDevice;
   TLabel *Label33;
   TComboBox *CB_SoundOutputDevice;
   TLabel *Lab_StatusMiddle;
        void __fastcall MI_QuitClick(TObject *Sender);
        void __fastcall MI_AboutClick(TObject *Sender);
        void __fastcall MI_DecodeWAVClick(TObject *Sender);
        void __fastcall FormShow(TObject *Sender);
        void __fastcall Btn_ClearDecoderOutputClick(TObject *Sender);
        void __fastcall MI_EncodeWAVClick(TObject *Sender);
        void __fastcall Timer1Timer(TObject *Sender);
        void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
        void __fastcall FormCreate(TObject *Sender);
        void __fastcall RxTxStopClick(TObject *Sender, TMouseButton Button,
          TShiftState Shift, int X, int Y);
        void __fastcall MI_ReceiveClick(TObject *Sender);
        void __fastcall MI_StopClick(TObject *Sender);
        void __fastcall MI_RXVolCtrlClick(TObject *Sender);
        void __fastcall MI_TXVolCtrlClick(TObject *Sender);
        void __fastcall FormResize(TObject *Sender);
        void __fastcall MI_OptionsClick(TObject *Sender);
        void __fastcall MI_TScope_WaterfallClick(TObject *Sender);
        void __fastcall MI_TScope_OffClick(TObject *Sender);
        void __fastcall MI_TScope_DecoderVarsClick(TObject *Sender);
        void __fastcall MI_QsoModeClick(TObject *Sender);
        void __fastcall MI_TScope_SpecGraphClick(TObject *Sender);
        void __fastcall MI_SendTuneToneClick(TObject *Sender);
        void __fastcall MI_RxFreqCheckClick(TObject *Sender);
        void __fastcall MI_WOLFsettingsClick(TObject *Sender);
        void __fastcall Btn_CopyDecoderOutputClick(TObject *Sender);
        void __fastcall Img_TuningMouseMove(TObject *Sender,
          TShiftState Shift, int X, int Y);
        void __fastcall SG_RxTxMsgsClick(TObject *Sender);
        void __fastcall Btn_RestartDecoderClick(TObject *Sender);
        void __fastcall MI_ScopeResLowClick(TObject *Sender);
        void __fastcall MI_ScopeResMedClick(TObject *Sender);
        void __fastcall MI_ScopeResHighClick(TObject *Sender);
        void __fastcall MI_TuningScopeClick(TObject *Sender);
        void __fastcall MI_ScopeAmplRangeClick(TObject *Sender);
        void __fastcall MI_ModeClick(TObject *Sender);
        void __fastcall MI_TxBeaconClick(TObject *Sender);
        void __fastcall MI_ManualClick(TObject *Sender);
        void __fastcall Chk_TestPTTClick(TObject *Sender);
        void __fastcall Chk_TestAlarmBellClick(TObject *Sender);
        void __fastcall MI_CP_LinradClick(TObject *Sender);
        void __fastcall MI_CP_SunriseClick(TObject *Sender);
        void __fastcall MI_QSOModeParamsClick(TObject *Sender);
        void __fastcall Btn_CalRxSrClick(TObject *Sender);
        void __fastcall MI_NormalSpeedClick(TObject *Sender);
        void __fastcall MI_HalfSpeedClick(TObject *Sender);
        void __fastcall MI_QuarterSpeedClick(TObject *Sender);
        void __fastcall MI_DoubleSpeedClick(TObject *Sender);
        void __fastcall MI_FourfoldSpeedClick(TObject *Sender);
        void __fastcall ApplySpecialOptions(TObject *Sender);
        void __fastcall MI_UsePrefilterClick(TObject *Sender);
   void __fastcall ApplyIOConfig(TObject *Sender);
private:	// User declarations
        int  m_iUpdating;
        Graphics::TBitmap *OffscreenBitmap;
        void __fastcall UpdateCaption(void);
        void __fastcall ShowBusyLed(int iTempRxTxState); // yellow LED
        int  __fastcall ApplySpecialOptions(void);
        void __fastcall UpdateConfig(void);        
        void __fastcall ApplyConfig(int iRxTxState);
        void __fastcall BuildRxCommandLine( char *pszWaveFileName );
        void __fastcall BeginReception( void );
        void __fastcall BuildTxCommandLine( char *pszWaveFileName );
        void __fastcall BeginTransmission( void );
        void __fastcall UpdateLevelIndicator(float fltInputLevelPercent);
        void __fastcall ClearTuningScope(void);
        void __fastcall GetTuningScopeParams( int *piNrFFTBins, int *piFirstFFTBinIndex, T_ScaleParams *pFSParams, T_ScaleParams *pASParams );
        void __fastcall UpdateTuningScope( float *pfltXr, float *pfltXi );
public:		// User declarations
        __fastcall TWolfMainForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TWolfMainForm *WolfMainForm;
//---------------------------------------------------------------------------
#endif
