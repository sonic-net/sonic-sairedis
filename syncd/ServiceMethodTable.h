#pragma once

extern "C"{
#include "sai.h"
}

#include "swss/logger.h"

#include <functional>
#include <vector>

namespace syncd
{
    class ServiceMethodTable
    {
        private:

            class SlotBase
            {
                public:

                    SlotBase(
                            _In_ sai_service_method_table_t smt);

                    virtual ~SlotBase();

                public:

                    void setHandler(
                            _In_ ServiceMethodTable* handler);

                    ServiceMethodTable* getHandler() const;

                    const sai_service_method_table_t& getServiceMethodTable() const;

                protected:

                    static const char* profileGetValue(
                            _In_ int context,
                            _In_ sai_switch_profile_id_t profile_id,
                            _In_ const char* variable);

                    static int profileGetNextValue(
                            _In_ int context,
                            _In_ sai_switch_profile_id_t profile_id,
                            _Out_ const char** variable,
                            _Out_ const char** value);

                protected:

                    ServiceMethodTable* m_handler;

                    sai_service_method_table_t m_smt;
            };

            template<size_t context>
                class Slot:
                    public SlotBase
        {
            public:

                Slot():
                    SlotBase({
                            .profile_get_value = &Slot<context>::profileGetValue,
                            .profile_get_next_value = &Slot<context>::profileGetNextValue
                            }) { }

                virtual ~Slot() {}

            private:

                static const char* profileGetValue(
                        _In_ sai_switch_profile_id_t profile_id,
                        _In_ const char* variable)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::profileGetValue(context, profile_id, variable);
                }

                static int profileGetNextValue(
                        _In_ sai_switch_profile_id_t profile_id,
                        _Out_ const char** variable,
                        _Out_ const char** value)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::profileGetNextValue(context, profile_id, variable, value);
                }
        };

            static std::vector<SlotBase*> m_slots;

        public:

            ServiceMethodTable();

            virtual ~ServiceMethodTable();

        public:

            const sai_service_method_table_t& getServiceMethodTable() const;

        public: // wrapped methods

            std::function<const char*(sai_switch_profile_id_t, const char*)> profileGetValue;

            std::function<int(sai_switch_profile_id_t, const char**, const char**)> profileGetNextValue;

        private:

            SlotBase*m_slot;
    };
}
