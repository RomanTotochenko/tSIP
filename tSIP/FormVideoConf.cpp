//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "FormVideoConf.h"
#include "VideoConf.h"
#include "ua/UaConf.h"
#include "ua/VideoModules.h"
#include "VideoDevicesList.h"
#include "Paths.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmVideoConf *frmVideoConf;
//---------------------------------------------------------------------------
__fastcall TfrmVideoConf::TfrmVideoConf(TComponent* Owner)
	: TForm(Owner),
	cfg(NULL)
{
    VideoModules::FillInputSelectorCb(cbInputMod);

	cbDisplayParentType->Clear();
	for (unsigned int i=0; i<VideoConf::DISPLAY_PARENT__LIMITER; i++)
	{
		cbDisplayParentType->Items->Add(VideoConf::getDisplayParentTypeName(static_cast<VideoConf::DisplayParentType>(i)));
	}

	VideoModules::FillOutputSelectorCb(cbOutputMod);
}
//---------------------------------------------------------------------------

void TfrmVideoConf::SetCfg(VideoConf *cfg, UaConf *uaCfg)
{
    VideoDevicesList::Instance().Refresh();

	this->cfg = cfg;
	this->uaCfg = uaCfg;

	chbEnabled->Checked = cfg->enabled;

	cbInputMod->ItemIndex = VideoModules::GetInputModuleCbIndex(uaCfg->video.videoSource.mod.c_str());
	cbInputModChange(NULL);

	if (uaCfg->video.videoSource.mod == VideoModules::avformat)
	{
		edInputFile->Text = uaCfg->video.videoSource.dev.c_str();
	}

	cbOutputMod->ItemIndex = VideoModules::GetOutputModuleCbIndex(uaCfg->video.videoDisplay.mod.c_str());
    cbOutputModChange(NULL);

	cbDisplayParentType->ItemIndex = cfg->displayParentType;
	edDisplayParentId->Text = cfg->displayParentId;

	edWidth->Text = uaCfg->video.width;
	edHeight->Text = uaCfg->video.height;
	edBitrate->Text = uaCfg->video.bitrate;
	edFps->Text = uaCfg->video.fps;

	chbSelfviewEnable->Checked = uaCfg->video.selfview.enabled;
	chbSelfviewPip->Checked = uaCfg->video.selfview.pip;

	chbDshowSkipReadingBackMediaFormat->Checked = uaCfg->video.dshow.skipReadingBackMediaFormat;
}

void TfrmVideoConf::Apply(void)
{
	cfg->enabled = chbEnabled->Checked;

	uaCfg->video.videoSource.mod = VideoModules::GetInputModuleFromCbIndex(cbInputMod->ItemIndex);
	if (uaCfg->video.videoSource.mod == VideoModules::avformat)
	{
		uaCfg->video.videoSource.dev = edInputFile->Text.c_str();
	}
	else
	{
		if (cbInputDev->Tag == 0 || cbInputDev->ItemIndex != cbInputDev->Items->Count - 1)
		{
			uaCfg->video.videoSource.dev = cbInputDev->Text.c_str();
		}
	}

	uaCfg->video.videoDisplay.mod = VideoModules::GetOutputModuleFromCbIndex(cbOutputMod->ItemIndex);

	cfg->displayParentType = static_cast<VideoConf::DisplayParentType>(cbDisplayParentType->ItemIndex);
	cfg->displayParentId = StrToIntDef(edDisplayParentId->Text, cfg->displayParentId);

	uaCfg->video.width = StrToIntDef(edWidth->Text, uaCfg->video.width);
	uaCfg->video.height = StrToIntDef(edHeight->Text, uaCfg->video.height);
	uaCfg->video.bitrate = StrToIntDef(edBitrate->Text, uaCfg->video.bitrate);
	uaCfg->video.fps = StrToIntDef(edFps->Text, uaCfg->video.fps);

	uaCfg->video.selfview.enabled = chbSelfviewEnable->Checked;
	uaCfg->video.selfview.pip = chbSelfviewPip->Checked;

	uaCfg->video.dshow.skipReadingBackMediaFormat = chbDshowSkipReadingBackMediaFormat->Checked;
}

void __fastcall TfrmVideoConf::cbInputModChange(TObject *Sender)
{
	AnsiString mod = VideoModules::GetInputModuleFromCbIndex(cbInputMod->ItemIndex);
	if (mod == VideoModules::dshow)
	{
		btnSelectInputFile->Visible = false;
		edInputFile->Visible = false;
		cbInputDev->Visible = true;
		lblInputDevice->Visible = true;
		VideoDevicesList::FillComboBox(cbInputDev, mod, false, uaCfg->video.videoSource.dev.c_str());
	}
	else if (mod == VideoModules::nullvideo ||
		mod == VideoModules::colorbar_generator ||
        mod == VideoModules::colorbar_generator_animated
		)
	{
		btnSelectInputFile->Visible = false;
		edInputFile->Visible = false;
		cbInputDev->Visible = false;
		lblInputDevice->Visible = false;
	}
	else if (mod == VideoModules::avformat)
	{
		btnSelectInputFile->Visible = true;
		edInputFile->Visible = true;
		cbInputDev->Visible = false;
		lblInputDevice->Visible = false;
	}
	else
	{
		assert(!"Unhandled cbSoundInputMod item index!");
		btnSelectInputFile->Visible = false;
		edInputFile->Visible = false;
		cbInputDev->Visible = false;
		lblInputDevice->Visible = false;
	}
}
//---------------------------------------------------------------------------
void __fastcall TfrmVideoConf::cbOutputModChange(TObject *Sender)
{
	AnsiString mod = VideoModules::GetInputModuleFromCbIndex(cbOutputMod->ItemIndex);
	if (mod == VideoModules::dshow)
	{
		lblDisplayParentType->Visible = true;
		cbDisplayParentType->Visible = true;
		lblDisplayParentId->Visible = true;
		edDisplayParentId->Visible = true;
	}
	else
	{
		lblDisplayParentType->Visible = false;
		cbDisplayParentType->Visible = false;
		lblDisplayParentId->Visible = false;
		edDisplayParentId->Visible = false;
	}
}
//---------------------------------------------------------------------------


void __fastcall TfrmVideoConf::btnSelectInputFileClick(TObject *Sender)
{
	int TODO__INITIAL_DIR_FAILS_ON_WIN10; // https://stackoverflow.com/questions/71595750/why-does-delphi-topendialog-fail-to-open-in-the-initial-directory
	dlgOpenDeviceFile->InitialDir = Paths::GetProfileDir();
	dlgOpenDeviceFile->Filter = "Video files|*.mp4;*.mkv;*.avi;*.mpg;*.wmv;*.flv;*.mov;*.rmvb;*.webm;*.3gp;*.asf|All files|*.*";
	if (edInputFile->Text != "")
	{
		if (FileExists(Paths::GetProfileDir() + "\\" + edInputFile->Text))
		{
			dlgOpenDeviceFile->FileName = Paths::GetProfileDir() + "\\" + edInputFile->Text;
		}
		else
		{
			dlgOpenDeviceFile->FileName = edInputFile->Text;
		}
	}
	if (dlgOpenDeviceFile->Execute())
	{
		if (UpperCase(Paths::GetProfileDir()) != UpperCase(ExtractFileDir(dlgOpenDeviceFile->FileName)))
		{
			edInputFile->Text = dlgOpenDeviceFile->FileName;
		}
		else
		{
			edInputFile->Text = ExtractFileName(dlgOpenDeviceFile->FileName);
		}
	}	
}
//---------------------------------------------------------------------------

