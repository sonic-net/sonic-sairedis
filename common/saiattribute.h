#ifndef __SAI_ATTRIBUTE__
#define __SAI_ATTRIBUTE__

#include <string>
#include <vector>

#include <hiredis/hiredis.h>
#include "swss/dbconnector.h"
#include "swss/table.h"
#include "swss/logger.h"
#include "sai.h"
#include "saiserialize.h"
#include "string.h"

class SaiAttribute
{
    public:

        SaiAttribute(
                _In_ const sai_object_type_t objectType,
                _In_ const swss::FieldValueTuple &value,
                _In_ bool onlyCount);

        ~SaiAttribute();

        sai_attribute_t* getAttr();

    private:

        SaiAttribute(const SaiAttribute&);
        SaiAttribute& operator=(const SaiAttribute&);

        bool m_onlyCount;

        sai_object_type_t m_objectType;
        sai_attribute_t m_attr;

        sai_attr_serialization_type_t m_serializationType;
};

#endif // __SAI_ATTRIBUTE__

