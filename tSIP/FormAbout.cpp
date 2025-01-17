//---------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "FormAbout.h"
#include "common\Utilities.h"
#include "Paths.h"
#include "Settings.h"
#include "baresip_base_config.h"
//---------------------------------------------------------------------
#pragma resource "*.dfm"
TfrmAbout *frmAbout = NULL;

//--------------------------------------------------------------------- 
__fastcall TfrmAbout::TfrmAbout(TComponent* AOwner)
	: TForm(AOwner)
{
	lblVersion->Caption = GetFileVer(Application->ExeName);
	lblBuildTimestamp->Caption = (AnsiString)__DATE__ + ", " __TIME__;
	ProductName->Caption = Application->Title;

#ifdef USE_VIDEO
	lblVideoState->Caption = "YES";
#else
	lblVideoState->Caption = "NO";
#endif

#ifdef __CODEGUARD__
	lblCodeGuardState->Caption = "YES";
#else
	lblCodeGuardState->Caption = "NO";
#endif

#ifdef _DEBUG
	lblDebugState->Caption = "YES";
#else
	lblDebugState->Caption = "NO";
#endif

	lblExeLocation->Caption = Application->ExeName;
	lblProfileDirLocation->Caption = Paths::GetProfileDir();	
}
//---------------------------------------------------------------------
void __fastcall TfrmAbout::lblUrlClick(TObject *Sender)
{
	TLabel* lbl = dynamic_cast<TLabel*>(Sender);
	if (!lbl)
		return;
	ShellExecute(NULL, "open", lbl->Caption.c_str(), NULL, NULL, SW_SHOWNORMAL);
}
//---------------------------------------------------------------------------

void __fastcall TfrmAbout::lblCodeGuardStateClick(TObject *Sender)
{
	// CG test
#ifdef __CODEGUARD__
	char overflowed[3];
	strcpy(overflowed, "test");
#endif
}
//---------------------------------------------------------------------------

void __fastcall TfrmAbout::lblExeLocationClick(TObject *Sender)
{
	ShellExecute(NULL, "explore", ExtractFileDir(Application->ExeName).c_str(), NULL, NULL, SW_SHOWNORMAL);
}
//---------------------------------------------------------------------------

void __fastcall TfrmAbout::lblProfileDirLocationClick(TObject *Sender)
{
	ShellExecute(NULL, "explore", Paths::GetProfileDir().c_str(), NULL, NULL, SW_SHOWNORMAL);
}
//---------------------------------------------------------------------------

void __fastcall TfrmAbout::FormKeyPress(TObject *Sender, char &Key)
{
	if (Key == VK_ESCAPE)
	{
		Close();
    }	
}
//---------------------------------------------------------------------------

void __fastcall TfrmAbout::FormShow(TObject *Sender)
{
	lblUrl->Caption = appSettings.branding.appUrl;	
}
//---------------------------------------------------------------------------

