//---------------------------------------------------------------------------


#pragma hdrstop

#include "AudioModules.h"
#include "baresip_base_config.h"

#include <json/json.h>
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>

//---------------------------------------------------------------------------

#pragma package(smart_init)

#define ARRAY_SIZE(a)    (sizeof(a)/sizeof(a[0]))

using namespace AudioModules;

const char* AudioModules::portaudio = "portaudio";
const char* AudioModules::winwave = "winwave";
const char* AudioModules::winwave2 = "winwave2";
const char* AudioModules::aufile = "aufile";
const char* AudioModules::aufileMm = "aufile_mm";
const char* AudioModules::nullaudio = "nullaudio";
const char* AudioModules::avformat = "avformat";

struct AudioModuleDef
{
	bool input;				///< handles audio input?
	bool output;        	///< handles audio output?
	const char* name;       ///< baresip name
	const char* description;///< UI name
};

const AudioModuleDef audioModuleDefs [] =
{
	{ true, true, portaudio, "PortAudio / DirectSound" },
	{ true, true, winwave2, "WaveIn, WaveOut" },
	{ true, false, aufile, "Wave file" },
	{ true, false, aufileMm, "Wave file - with MM timer" },
#ifdef USE_VIDEO
	{ true, false, avformat, "FFmpeg avformat (mp3, aac, http radio, ...)" },
#endif
	{ true, true, nullaudio, "Null (silence)" },
};
	

void AudioModules::FillInputSelectorCb(Stdctrls::TComboBox *cb)
{
	cb->Items->Clear();
	for (unsigned int i=0; i<ARRAY_SIZE(audioModuleDefs); i++)
	{
		const AudioModuleDef &mod = audioModuleDefs[i];
		if (mod.input)
		{
			cb->Items->Add(mod.description);
		}
	}
}

void AudioModules::FillOutputSelectorCb(Stdctrls::TComboBox *cb)
{
	cb->Items->Clear();
	for (unsigned int i=0; i<ARRAY_SIZE(audioModuleDefs); i++)
	{
		const AudioModuleDef &mod = audioModuleDefs[i];
		if (mod.output)
		{
			cb->Items->Add(mod.description);
		}
	}
}

const char* AudioModules::GetInputModuleFromCbIndex(int id)
{
	int idCounter = -1;
	for (unsigned int i=0; i<ARRAY_SIZE(audioModuleDefs); i++)
	{
		const AudioModuleDef &mod = audioModuleDefs[i];
		if (mod.input)
		{
			idCounter++;
			if (idCounter == id)
			{
				return mod.name;
			}
		}
	}
	assert(!"Unhandled module index!");
	return portaudio;
}

int AudioModules::GetInputModuleCbIndex(const std::string& name)
{
	int idCounter = -1;
	for (unsigned int i=0; i<ARRAY_SIZE(audioModuleDefs); i++)
	{
		const AudioModuleDef &mod = audioModuleDefs[i];
		if (mod.input)
		{
			idCounter++;
			if (name == mod.name)
			{
				return idCounter;
			}
		}
	}
	assert(!"Unhandled module name!");
	return 0;
}


const char* AudioModules::GetOutputModuleFromCbIndex(int id)
{
	int idCounter = -1;
	for (unsigned int i=0; i<ARRAY_SIZE(audioModuleDefs); i++)
	{
		const AudioModuleDef &mod = audioModuleDefs[i];
		if (mod.output)
		{
			idCounter++;
			if (idCounter == id)
			{
				return mod.name;
			}
		}
	}
	assert(!"Unhandled module index!");
	return portaudio;
}

int AudioModules::GetOutputModuleCbIndex(const std::string& name)
{
	int idCounter = -1;
	for (unsigned int i=0; i<ARRAY_SIZE(audioModuleDefs); i++)
	{
		const AudioModuleDef &mod = audioModuleDefs[i];
		if (mod.output)
		{
			idCounter++;
			if (name == mod.name)
			{
				return idCounter;
			}
		}
	}
	assert(!"Unhandled module name!");
	return 0;
}

bool AudioModules::IsInput(const std::string& name)
{
	for (unsigned int i=0; i<ARRAY_SIZE(audioModuleDefs); i++)
	{
		const AudioModuleDef &mod = audioModuleDefs[i];
		if (mod.input)
		{
			if (name == mod.name)
			{
				return true;
			}
		}
	}
	return false;
}

bool AudioModules::IsOutput(const std::string& name)
{
	for (unsigned int i=0; i<ARRAY_SIZE(audioModuleDefs); i++)
	{
		const AudioModuleDef &mod = audioModuleDefs[i];
		if (mod.output)
		{
			if (name == mod.name)
			{
				return true;
			}
		}
	}
	return false;
}

void AudioModules::GetModuleFromJson(const Json::Value &jv, const std::string &name, enum Dir dir, std::string &mod)
{
	std::string tmp = mod;
	jv.getString(name.c_str(), tmp);
	if (tmp == winwave)
		tmp = winwave2;
	if (dir == DIR_INPUT)
	{
		if (AudioModules::IsInput(tmp))
		{
			mod = tmp;
		}
	}
	else if (dir == DIR_OUTPUT)
	{
		if (AudioModules::IsOutput(tmp))
		{
			mod = tmp;
		}
	}
}


