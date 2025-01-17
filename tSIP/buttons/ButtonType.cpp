//---------------------------------------------------------------------------


#pragma hdrstop

#include "ButtonType.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)

using namespace Button;

const char* Button::TypeName(enum Type type)
{
	switch (type)
	{
	case DISABLED:
		return "Disabled";
	case SPEED_DIAL:
		return "Speed Dial";
	case BLF:
		return "BLF";
	case DTMF:
		return "DTMF";
	case REDIAL:
		return "Redial";
	case TRANSFER:
		return "Blind transfer";
	case HOLD:
		return "Hold";
	case REREGISTER:
		return "Re-register";
	case MWI:
		return "MWI";
	case MUTE:
		return "Mute";
	case MUTE_RING:
		return "Mute ring";
	case CONTACT_NOTE:
		return "Contact note";
	case HTTP_QUERY:
		return "HTTP query";
	case PRESENCE:
		return "Presence";
	case UNREGISTER:
		return "Unregister";
	case EXECUTE:
		return "Execute";
	case PAGING_TX:
		return "RTP streaming (paging)";
	case SCRIPT:
		return "Script (Lua)";
	case SIP_ACCESS_URL:
		return "Access SIP URL";
	case SWITCH_AUDIO_SOURCE:
		return "Switch audio source";
	case SWITCH_AUDIO_PLAYER:
		return "Switch audio output";
	case HANGUP:
		return "Hangup";
	case SHOW_SETTINGS:
		return "Show settings";
	case MINIMIZE:
		return "Minimize application";
	case SHOW_LOG:
		return "Show log";
	case EXIT:
		return "Exit";
	case CONTACT_FILE:
		return "Contact file";
	case UA_RESTART:
		return "Restart user agent";
	case SEND_TEXT_MESSAGE:
		return "Send text message";
	case RECORD:
		return "Start/resume recording";
	case RECORD_PAUSE:
		return "Pause recording";
	case AUTO_ANSWER_DND:
		return "Auto answer/DND";
	case ZRTP_VERIFY_SAS:
		return "ZRTP: verify SAS";
	case ZRTP_UNVERIFY_SAS:
		return "ZRTP: unverify SAS";
	case SWITCH_VIDEO_SOURCE:
		return "Switch video source";
	case LINE:
		return "LINE - multiple calls";
	case HANGUP_ALL:
		return "Hangup all calls";
	case ATTENDED_TRANSFER:
		return "Attended transfer";
	case CONFERENCE_START:
		return "Start conference";
	case CALL_MAKE_OR_ANSWER:
		return "Make or answer the call";
	case CALL_ANSWER:
		return "Answer the current call";

	default:
		return "???";
	}
}
const char* Button::TypeDescription(enum Type type)
{
	switch (type)
	{
	case DISABLED:
		return "Button disabled - no action.";
	case SPEED_DIAL:
		return "Quick dialing.";
	case BLF:
		return "Busy Lamp Field - show peer call state (dialog subscription).";
	case DTMF:
		return "Send digits during call or add digits to dialed number. Accepted: 0-9, *, #, A, B, C, D and also R = FLASH.";
	case REDIAL:
		return "Dial last number again.";
	case TRANSFER:
		return "Blind transfer, using the number from button configuration as target or if this number is empty - number from the main window (\"Transfer to ...\").";
	case HOLD:
		return "Temporary disable media stream.";
	case REREGISTER:
		return "Repeat registration immediately (speed up recovery after server restart).";
	case MWI:
		return "Voice mail: Message Waiting Indicator (unsolicited). Uses \"early\" icon if new messages are present.";
	case MUTE:
		return "Mute outgoing audio during call.";
	case MUTE_RING:
		return "Mute ring on incoming calls.";
	case CONTACT_NOTE:
		return "Show/edit note of the last contact.";
	case HTTP_QUERY:
		return "Open browser - query for number from last incoming call.";
	case PRESENCE:
		return "Presence (application/pidf+xml). Use \"terminated\" bitmap for Online state, \"early\" bitmap for Offline, \"confirmed\" bitmap for Busy.";
	case UNREGISTER:
		return "Unregister (leave registration server).";
	case EXECUTE:
		return "Start other application (e.g. RecordViewer). Number specifies executable, Argument1 specifies arguments passed to application and \"[number]\" if present will be substituted with number from last call.";
	case PAGING_TX:
		return "Start RTP streaming (paging) to specified address (multicast, unicast) in Number edit (e.g. 192.168.1.10:4000). If wave file is not specified default audio source (same as when calling) is used.";
	case SCRIPT:
		return "Execute Lua script from specified file (uses \"scripts\" subdirectory).";
	case SIP_ACCESS_URL:
		return "Open URL specified by \"Access-URL\" header from incoming INVITE.";
	case SWITCH_AUDIO_SOURCE:
		return "Switch audio source device for the current call or RTP streaming.";
	case SWITCH_AUDIO_PLAYER:
		return "Switch audio output device for the current call.";
	case HANGUP:
		return "Terminate current call or RTP streaming (paging).";
	case SHOW_SETTINGS:
		return "Show application settings window (to be used in kiosk applications and/or if main menu is hidden).";
	case MINIMIZE:
		return "Minimize application window.";
	case SHOW_LOG:
		return "Show application log.";
	case EXIT:
		return "Close application.";
	case CONTACT_FILE:
		return "Open file associated with contact / phonebook entry.";
	case UA_RESTART:
		return "Restart user agent (core part of the softphone, working thread) to e.g. speed up BLF refreshing if PABX was restarted.";
	case SEND_TEXT_MESSAGE:
		return "Send text message (Instant Messaging, SIMPLE) to number/URI defined with button or (if button URI is empty) - number/URI from main window dial edit.";
	case RECORD:
		return "Start recording with default settings (if recording is set to manual) or resume paused recording.";
	case RECORD_PAUSE:
		return "Pause current recording.";
	case AUTO_ANSWER_DND:
		return "Auto answer or DND. For auto answer use SIP code = 200, for DND = 486 or 603.";
	case ZRTP_VERIFY_SAS:
		return "Verify SAS (Short Authentication String) for current ZRTP session.";
	case ZRTP_UNVERIFY_SAS:
		return "Remove SAS (Short Authentication String) verification for current ZRTP session.";
	case SWITCH_VIDEO_SOURCE:
		return "Switch video source (module + device) for the current call, switching to e.g. DirectShow camera, avformat mp4 file or test video generator.";
	case LINE:
		return "LINE buttons are used to manage multiple simultaneous calls. Each button represents single call and maximum call number would be limited by configured number of LINE buttons. If no button is defined, application is limited to single call.";
	case HANGUP_ALL:
		return "Terminate all currently active calls.";		
	case ATTENDED_TRANSFER:
		return "Make attended transfer using two already active calls.";
	case CONFERENCE_START:
		return "Create conference from current calls.";
	case CALL_MAKE_OR_ANSWER:
		return "Make a new call or answer the current call (like default Call/Answer button in the main window).";
	case CALL_ANSWER:
		return "Answer the current call (like default Call/Answer button but only for incoming call).";

	default:
		return "???";
	}
}
