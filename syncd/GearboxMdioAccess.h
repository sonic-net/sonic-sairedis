#pragma once

#include "swss/dbconnector.h"
#include "swss/table.h"

#include <map>
#include <memory>
#include <string>

extern "C" {
#include "sai.h"
}

namespace syncd
{
    /**
     * @brief Supplies the MDIO register access callbacks a gearbox PHY's SAI needs.
     */
    class GearboxMdioAccess
    {
        public:

            GearboxMdioAccess();

            virtual ~GearboxMdioAccess();

        public:

            /**
             * @brief Point the MDIO access attributes at this process's symbols.
             */
            void updateMdioPointers(
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t* attr_list);

        private:

            struct MdioSymbols
            {
                void* read;
                void* write;
            };

            /**
             * @brief Load the library serving this PHY and resolve its symbols.
             */
            const MdioSymbols* getSymbols(
                    _In_ const std::string& hwinfo);

            /**
             * @brief Read the library path each PHY is configured with.
             *
             * Deferred until a PHY switch is created: orchagent only issues that
             * create from this same table, so by then gearsyncd has published it.
             */
            void loadLibraryNames();

            /**
             * @brief Read the hardware info attribute, which keys the table.
             */
            static bool getHardwareInfo(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t* attr_list,
                    _Out_ std::string& hwinfo);

        private:

            std::shared_ptr<swss::DBConnector> m_applDb;

            std::shared_ptr<swss::Table> m_gearboxTable;

            bool m_loaded;

            std::map<std::string, std::string> m_libraryByHwinfo;

            std::map<std::string, MdioSymbols> m_symbolsByLibrary;
    };
}
