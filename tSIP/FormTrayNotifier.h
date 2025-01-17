//---------------------------------------------------------------------------

#ifndef FormTrayNotifierH
#define FormTrayNotifierH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------

#include "Callback.h"

class TfrmTrayNotifier : public TForm
{
__published:	// IDE-managed Components
	TLabel *lblDescription;
	TLabel *lblUri;
	TButton *btnAnswer;
	TButton *btnHangup;
	TButton *btnStopFocus;
	void __fastcall btnHangupClick(TObject *Sender);
	void __fastcall btnAnswerClick(TObject *Sender);
	void __fastcall FormCreate(TObject *Sender);
	void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
private:	// User declarations
	static void TranslateForm(void* obj);
	typedef void(__closure *CallbackHangup)(unsigned int uid, int sipCode, AnsiString reason);
	typedef void(__closure *CallbackAnswer)(unsigned int uid);
	void __fastcall Close(void);
	void __fastcall CreateParams(TCreateParams &Params);
	unsigned int callUid;	
public:		// User declarations
	__fastcall TfrmTrayNotifier(TComponent* Owner);
	void SetData(unsigned int callUid, AnsiString description, AnsiString uri, bool incoming);
	void ShowWithoutFocus(void);
    void HideWindow(void);
	CallbackHangup OnHangup;
	CallbackAnswer OnAnswer;
	void SetCallState(Callback::ua_state_e state);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmTrayNotifier *frmTrayNotifier;
//---------------------------------------------------------------------------
#endif
