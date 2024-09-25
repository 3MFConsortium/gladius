/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is the class declaration of CResourceAccessor

*/


#ifndef __GLADIUSLIB_RESOURCEACCESSOR
#define __GLADIUSLIB_RESOURCEACCESSOR

#include "gladius_interfaces.hpp"

// Parent classes
#include "gladius_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.


namespace GladiusLib {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CResourceAccessor 
**************************************************************************************************************************/

class CResourceAccessor : public virtual IResourceAccessor, public virtual CBase {
private:

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


    /**
    * Public member functions to implement.
    */

    GladiusLib_uint64 GetSize() override;

    bool Next() override;

    bool Prev() override;

	void Begin() override;
};

} // namespace Impl
} // namespace GladiusLib

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __GLADIUSLIB_RESOURCEACCESSOR
