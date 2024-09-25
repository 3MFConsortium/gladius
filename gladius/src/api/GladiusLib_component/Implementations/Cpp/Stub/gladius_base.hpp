/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is the class declaration of CBase

*/


#ifndef __GLADIUSLIB_BASE
#define __GLADIUSLIB_BASE

#include "gladius_interfaces.hpp"
#include <vector>
#include <list>
#include <memory>


// Include custom headers here.


namespace GladiusLib {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CBase 
**************************************************************************************************************************/

class CBase : public virtual IBase {
private:

	std::unique_ptr<std::list<std::string>> m_pErrors;
	GladiusLib_uint32 m_nReferenceCount = 1;

	/**
	* Put private members here.
	*/

protected:

	/**
	* Put protected members here.
	*/

public:

	/**
	* Put additional public members here. They will not be visible in the external API.
	*/

	bool GetLastErrorMessage(std::string & sErrorMessage) override;

	void ClearErrorMessages() override;

	void RegisterErrorMessage(const std::string & sErrorMessage) override;

	void IncRefCount() override;

	bool DecRefCount() override;


	/**
	* Public member functions to implement.
	*/

};

} // namespace Impl
} // namespace GladiusLib

#endif // __GLADIUSLIB_BASE
