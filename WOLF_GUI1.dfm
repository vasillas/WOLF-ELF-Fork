�
 TWOLFMAINFORM 0�6  TPF0TWolfMainFormWolfMainFormLeft� Topr
AutoScrollCaptionWOLFClientHeightjClientWidth�Color	clBtnFaceConstraints.MinHeight,Constraints.MinWidthTFont.CharsetDEFAULT_CHARSET
Font.ColorclWindowTextFont.Height�	Font.NameMS Sans Serif
Font.Style Menu	MainMenu1OldCreateOrderPositionpoScreenCenterScaledOnClose	FormCloseOnCreate
FormCreateOnResize
FormResizeOnShowFormShowPixelsPerInch`
TextHeight TPageControlPageControl1Left Top Width�HeightY
ActivePageTS_MainScreenAlignalClientTabIndex TabOrder  	TTabSheetTS_MainScreenCaptionMain screen 	TSplitter	Splitter1Left TopiWidth�HeightCursorcrVSplitAlignalTop  
TScrollBoxSCB_TxLeft Top Width�HeightiHorzScrollBar.VisibleVertScrollBar.VisibleAlignalTop
AutoScrollConstraints.MinHeight(TabOrder 
DesignSize�e  TLabelLabel1LeftTTopWidth$HeightCaptionTX msg  TImage
Img_TuningLeftTop(Width�Height1AnchorsakLeftakTopakRightakBottom Constraints.MinHeight Constraints.MinWidthdOnMouseMoveImg_TuningMouseMove  TImageImg_InputLevelLeft Top Width	HeightYHintinput levelAnchorsakLeftakTopakBottom ParentShowHintShowHint	  TLabelLab_CursorInfo1Left@TopWidth=HeightCaption<cursor info>  TLabelLab_CursorInfo2LeftCTopWidthHeightCaption-  TLabelLabel29Left4TopWidth
HeightCaptionC:  TLabelLabel30Left�TopWidth
HeightCaptionP:  TLabelLab_PkInfo1Left�TopWidth-HeightCaption<peak_f>  TLabelLab_PkInfo2Left�TopWidthHeightCaption-  TEditEd_TxStringLeft|TopWidth� HeightFont.CharsetANSI_CHARSET
Font.ColorclWindowTextFont.Height�	Font.NameCourier New
Font.Style 	MaxLength
ParentFontTabOrder TextQUICK BROWN FOX  TPanelPnl_RxTxOffLeftTopWidthEHeight$	AlignmenttaLeftJustifyFont.CharsetANSI_CHARSET
Font.ColorclWindowTextFont.Height�	Font.NameArial
Font.StylefsBold 
ParentFontTabOrderOnMouseDownRxTxStopClick TLabelLab_RxTxInfo2LeftTopWidthHeightCaptionF9:RXFont.CharsetANSI_CHARSET
Font.ColorclWindowTextFont.Height�	Font.NameArial
Font.Style 
ParentFontOnMouseDownRxTxStopClick  TLabelLab_RxTxInfo1LeftTopWidthHeightCaptionOFFOnMouseDownRxTxStopClick  TShapeShp_RxTxLedLeft(TopWidthHeightBrush.ColorclLime	Pen.WidthShapestCircleOnMouseDownRxTxStopClick    
TScrollBoxSCB_RxLeft TopnWidth�Height� AlignalClientParentShowHintShowHint	TabOrder
DesignSize��   TLabelLabel3LeftTopWidthLHeightCaptionDecoder Output  TButtonBtn_ClearDecoderOutputLeft�TopWidth)HeightHintclear decoder output windowAnchorsakTopakRight CaptionClearTabOrder OnClickBtn_ClearDecoderOutputClick  	TRichEditREd_DecoderLeft TopWidth�Height� AnchorsakLeftakTopakRightakBottom Font.CharsetANSI_CHARSET
Font.ColorclWindowTextFont.Height�	Font.NameCourier New
Font.Style HideScrollBars
ParentFont
ScrollBarsssBothTabOrderWordWrap  TButtonBtn_CopyDecoderOutputLeftiTopWidth)HeightHint copy output as text to clipboardAnchorsakTopakRight CaptionCopyTabOrderOnClickBtn_CopyDecoderOutputClick  TButtonBtn_RestartDecoderLeft=TopWidth)HeightHint%Resets the decoder, begins new decodeAnchorsakTopakRight CaptionRestartTabOrderOnClickBtn_RestartDecoderClick    	TTabSheetTS_WolfConfigCaptionWOLF Config
ImageIndexParentShowHintShowHint	 	TGroupBoxGrp_TxOptionsLeftTopLWidth� Height� CaptionTransmit OptionsTabOrder  TLabelLabel12LeftTopdWidthwHeightCaptionPhase reversal time (0..1)  TLabelLabel13LeftTopLWidthnHeightCaptionOutput attenuation (dB)  TLabelLabel6Left� TopWidthHeightCaptionHz  TLabelLabel4LeftTopWidth8HeightCaptionSample rate  TLabelLabel10Left� Top,WidthHeightCaptionHz  TLabelLabel5LeftTop,Width4HeightCaptionCenter freq  TEditEd_TxPhaseRevTimeLeft� Top`Width-HeightTabOrder Text0.1  TEditEd_OutputAttenuationLeft� TopHWidth-HeightTabOrderText8  TEditEd_TxSampleRateLeftDTopWidthEHeightTabOrderText8000  TEditEd_TxFcenterLeftDTop(WidthEHeightTabOrderText800   	TGroupBox
Grp_RxOptsLeft� TopLWidth� Height� CaptionReceive OptionsTabOrder TLabelLabel15LeftTopLWidthXHeightCaptionSlew rate (Hz/sec)  TLabelLabel16LeftTopdWidthRHeightCaptionFreqcy. tolerance  TLabelLabel17Left� TopdWidthHeightCaptionHz  TLabelLabel26LeftTop|Width� HeightCaptionNr of frames (a 96 seconds)   TLabelLabel27Left,Top� Width^HeightCaptionper decoding period  TLabelLabel7Left� TopWidthHeightCaptionHz  TLabelLabel11Left� Top,WidthHeightCaptionHz  TLabelLabel9LeftTopWidth8HeightCaptionSample rate  TLabelLabel28LeftTop,Width4HeightCaptionCenter freq  TEditEd_SlewRateLefttTopHWidthIHeightTabOrder Text0.0000  TEditEd_FreqTolerLefttTop`Width)HeightTabOrderText1.0  	TCheckBoxCHK_RestartDecoderLeftTop� Width� HeightHint9..which happens after 51 minutes when using the soundcardCaptionRestart decoder when finishedTabOrder  TEditEd_NrRxFramesLeft� Top� WidthHeightTabOrderText32  TUpDownUD_NrRxFramesLeft� Top� WidthHeight	AssociateEd_NrRxFramesMinMax Position TabOrder	ThousandsWrap  TEditEd_RxSampleRateLeft@TopWidthEHeightTabOrderText8000  TEditEd_RxFcenterLeft@Top(WidthEHeightTabOrderText800  TButtonBtn_CalRxSrLeft� TopWidthHeightCaptionCALTabOrderOnClickBtn_CalRxSrClick   	TGroupBoxGRP_QSOModeLeftTop Width�HeightECaptionQSO ModeTabOrder TLabelLabel18LeftTopWidthHeightCaptionPeriod  TLabelLabel19LeftXTopWidth$HeightCaptionminutes  TLabelLabel20Left� TopWidthEHeightCaptionMy TX period :  TLabelLabel25Left4Top,Width6HeightCaptionARemaining TX-overs until stopping own transmission (= "tc" value)  TEditEd_PeriodMinsLeft4TopWidthHeightTabOrder Text15  TRadioButton
RB_TxFirstLeft� TopWidth-HeightCaptionfirstTabOrder  TRadioButton	RB_TxLastLeftTopWidthAHeightCaptionsecondTabOrder  TEditEd_TxOversLeftLeftTop(WidthHeightTabOrderText50    	TTabSheetTS_SpecialsCaptionSpecial
ImageIndex TLabelLabel14LeftToplWidth� HeightCaption/Experimental options (see WOLF documentation) :  TLabelLabel2LeftTop8Width� HeightCaption)Frequency measuring interval ('m' option)  TLabelLabel8Left� Top8WidthHeightCaptions  TLabelLabel31Left� TopPWidth6HeightCaptiondB/sqrt(Hz)  TCheckListBoxCLB_SpecialOptsLeftTop|WidthaHeightU
ItemHeightTabOrder   	TCheckBoxCHK_VerboseMsgsLeftTop Width� HeightCaptionVerbose messages from decoder TabOrder  TEditEd_FreqMeasIntvLeft� Top4WidthHeightTabOrderText96  	TGroupBoxGrp_AudioFilesLeft� TopWidthqHeight=CaptionAudio File FormatTabOrder TRadioButtonRB_8BitWaveLeftTopWidthaHeightCaption8 bit / sampleTabOrder   TRadioButtonRB_16BitWaveLeftTop$WidthaHeightCaption16 bit / sampleTabOrder   	TCheckBoxCHK_ShowTimeOfDayLeftTopWidth� HeightCaption%Show time of day in each decoded lineTabOrderOnClickApplySpecialOptions  	TCheckBoxChk_AddNoiseOnRxLeftTopPWidth� HeightCaptionTEST: Add noise on RX, level:TabOrderOnClickApplySpecialOptions  TEditEd_AddedNoiseLevelLeft� TopLWidthHeightTabOrderText-40OnChangeApplySpecialOptions   	TTabSheetTS_IO_ConfigCaption
I/O Config
ImageIndexParentShowHintShowHint	 TLabelLabel23LeftTopWidth� HeightCaptionCOM Port for RX/TX control  TLabelLabel24LeftTop0WidthHeightCaptionTest:  TLabelLabel32LeftTopPWidth[HeightCaptionInput Audio Device  TLabelLabel33LeftToplWidthcHeightCaptionOutput Audio Device  	TComboBoxCB_ComPortNrLeft� TopWidthAHeightStylecsDropDownList
ItemHeightTabOrder Items.StringsnoneCOM1COM2COM3COM4COM5COM6COM7COM8   	TCheckBoxChk_TestPTTLeft(Top0WidthMHeightHintRTS = Request To Send (!)Caption	PTT (RTS)TabOrderOnClickChk_TestPTTClick  	TCheckBoxChk_TestAlarmBellLeft� Top0WidtheHeightHintDTR = Data Terminal Ready (!)CaptionAlarm bell (DTR)TabOrderOnClickChk_TestAlarmBellClick  	TComboBoxCB_SoundInputDeviceLeftpTopLWidth� HeightStylecsDropDownList
ItemHeight TabOrderOnChangeApplyIOConfig  	TComboBoxCB_SoundOutputDeviceLeftpTophWidth� HeightStylecsDropDownList
ItemHeight TabOrderOnChangeApplyIOConfig   	TTabSheetTS_AAMachineCaptionRX / TX messages
ImageIndex 
TScrollBox
ScrollBox1Left Top Width�Height9AlignalTop
AutoScrollParentShowHintShowHint	TabOrder TLabelLabel21Left� TopWidth&HeightCaptionmycall =  TLabelLabel22LeftTopWidth$HeightCaptiondxcall =  	TCheckBoxChk_AAMachineEnabledLeftTopWidth� HeightCaptionauto-answer enabledTabOrder   TEdit	Ed_MyCallLeft� TopWidth9HeightTabOrder  TEdit	Ed_DxCallLeft@TopWidth9HeightTabOrder  	TCheckBoxCHK_AlarmOnReceptionLeftTopWidthMHeightCaption>Ring the alarm bell if any of these messages has been receivedTabOrder   TStringGridSG_RxTxMsgsLeft Top9Width�HeightAlignalClientColCountDefaultColWidth(RowCountFont.CharsetANSI_CHARSET
Font.ColorclWindowTextFont.Height�	Font.NameCourier New
Font.Style OptionsgoFixedVertLinegoFixedHorzLine
goVertLine
goHorzLinegoRangeSelectgoColSizing	goEditinggoThumbTracking 
ParentFontTabOrder OnClickSG_RxTxMsgsClick	ColWidths(� � f     
TScrollBox
SCB_StatusLeft TopYWidth�HeightAlignalBottomTabOrder TLabel
Lab_StatusLeft Top Width6HeightAlignalLeftCaptionInitialising...  TLabelLab_StatusRightLeft8Top Width� HeightAlignalRight	AlignmenttaRightJustifyAutoSizeCaption-  TLabelLab_StatusMiddleLeft� Top Width,HeightCaptionhh:mm:ss   	TMainMenu	MainMenu1LeftX 	TMenuItemMI_FileCaption&File 	TMenuItemMI_DecodeWAVCaption&Decode WAVE-fileOnClickMI_DecodeWAVClick  	TMenuItemMI_EncodeWAVCaption&Encode WAVE-fileOnClickMI_EncodeWAVClick  	TMenuItemN1Caption-  	TMenuItemMI_QuitCaption&QuitOnClickMI_QuitClick   	TMenuItemMI_ModeCaption&ModeOnClickMI_ModeClick 	TMenuItem
MI_ReceiveCaption&Receive onlyShortCutxOnClickMI_ReceiveClick  	TMenuItemMI_TxBeaconCaption(&Transmit only (beacon, immediate start)ShortCutyOnClickMI_TxBeaconClick  	TMenuItemMI_StopCaption&StopShortCutzOnClickMI_StopClick  	TMenuItemN4Caption-  	TMenuItem
MI_QsoModeCaption&QSO mode (RX + TX)ShortCut{OnClickMI_QsoModeClick  	TMenuItemMI_QSOModeParamsCaptionQSO mode parameters ...OnClickMI_QSOModeParamsClick  	TMenuItemMI_WolfSpeedCaption<speed> 	TMenuItemMI_NormalSpeedCaption&Normal speed (10 bit/sec)OnClickMI_NormalSpeedClick  	TMenuItemMI_HalfSpeedCaption&Half speed (5 bit/sec)OnClickMI_HalfSpeedClick  	TMenuItemMI_QuarterSpeedCaption&Quarter speed (2.5 bit/sec)OnClickMI_QuarterSpeedClick  	TMenuItemN6Caption-  	TMenuItemMI_DoubleSpeedCaption&Double speed (20 bit/sec)OnClickMI_DoubleSpeedClick  	TMenuItemMI_FourfoldSpeedCaption&Fourfold speed (40 bit/sec)OnClickMI_FourfoldSpeedClick   	TMenuItemN2Caption-  	TMenuItemMI_SendTuneToneCaption#Transmit Tuning tone (&unmodulated)OnClickMI_SendTuneToneClick  	TMenuItemMI_RxFreqCheckCaption'&Frequency measurement (RX-drift check)OnClickMI_RxFreqCheckClick   	TMenuItemMI_TuningScopeCaptionTuning scopeOnClickMI_TuningScopeClick 	TMenuItemMI_TScope_WaterfallCaption
&WaterfallOnClickMI_TScope_WaterfallClick  	TMenuItemMI_TScope_SpecGraphCaption&Spectum GraphOnClickMI_TScope_SpecGraphClick  	TMenuItemMI_TScope_DecoderVarsCaption&Plot decoder variablesOnClickMI_TScope_DecoderVarsClick  	TMenuItemMI_TScope_OffCaptionTuning scope &OFFOnClickMI_TScope_OffClick  	TMenuItemN3Caption-  	TMenuItemMI_ScopeResCaption	&Freq res 	TMenuItemMI_ScopeResLowCaption&Low      (8192 point FFT)OnClickMI_ScopeResLowClick  	TMenuItemMI_ScopeResMedCaption&Medium (16384 point FFT)OnClickMI_ScopeResMedClick  	TMenuItemMI_ScopeResHighCaption&High      (32768 point FFT)OnClickMI_ScopeResHighClick   	TMenuItemMI_ScopeAmplRangeCaption&Ampl rangeOnClickMI_ScopeAmplRangeClick  	TMenuItemMI_WfColorPalCaption&Color palette 	TMenuItemMI_CP_LinradCaption&LinradOnClickMI_CP_LinradClick  	TMenuItemMI_CP_SunriseCaptionSun&riseOnClickMI_CP_SunriseClick    	TMenuItem
MI_OptionsCaption&OptionsOnClickMI_OptionsClick 	TMenuItemMI_WOLFsettingsCaption&WOLF decoder settings ...OnClickMI_WOLFsettingsClick  	TMenuItemMI_UsePrefilterCaptionUse experimental &PRE-FILTEROnClickMI_UsePrefilterClick  	TMenuItemN7Caption-  	TMenuItemMI_RXVolCtrlCaption(&RX Volume control (soundcard recording)OnClickMI_RXVolCtrlClick  	TMenuItemMI_TXVolCtrlCaption%&TX Volume control (soundcard output)OnClickMI_TXVolCtrlClick   	TMenuItemMI_HelpCaption&Help 	TMenuItemMI_AboutCaption&AboutOnClickMI_AboutClick  	TMenuItemN5Caption-  	TMenuItem	MI_ManualCaptionWOLF GUI &manualOnClickMI_ManualClick    TOpenDialogOpenDialog1LeftpTop��    TSaveDialogSaveDialog1Left�Top��    TTimerTimer1IntervaldOnTimerTimer1TimerLeft�Top��     