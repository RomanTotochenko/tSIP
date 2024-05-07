//---------------------------------------------------------------------------

#ifndef ButtonContainerConfH
#define ButtonContainerConfH
//---------------------------------------------------------------------------

#include <System.hpp>

namespace Json
{
	class Value;
}

class ButtonContainerConf
{
public:
	AnsiString backgroundImage;
	bool backgroundImageTransparent;
	int backgroundColor;
	
	ButtonContainerConf(void);

	bool operator==(const ButtonContainerConf& right) const {
		return (
			backgroundImage == right.backgroundImage &&
			backgroundImageTransparent == right.backgroundImageTransparent &&
			backgroundColor == right.backgroundColor
		);
	}
	bool operator!=(const ButtonContainerConf& right) const {
    	return !(*this == right);
	}

	void FromJson(const Json::Value &jv);
	void ToJson(Json::Value &jv) const;
};

#endif
