#include "GearboxMdioAccess.h"
#include "swss/logger.h"
#include "swss/tokenize.h"
#include <dlfcn.h>

using namespace syncd;

#define MDIO_READ_SYMBOL  "mdio_read"
#define MDIO_WRITE_SYMBOL "mdio_write"

#define GEARBOX_TABLE_NAME "_GEARBOX_TABLE"

GearboxMdioAccess::GearboxMdioAccess():
    m_loaded(false)
{
    SWSS_LOG_ENTER();
}

GearboxMdioAccess::~GearboxMdioAccess()
{
    SWSS_LOG_ENTER();

    /* Library handles are never closed: the vendor SAI may still call through these while it shuts down. */
}

void GearboxMdioAccess::loadLibraryNames()
{
    SWSS_LOG_ENTER();

    m_loaded = true;

    try
    {
        m_applDb = std::make_shared<swss::DBConnector>("APPL_DB", 0);
        m_gearboxTable = std::make_shared<swss::Table>(m_applDb.get(), GEARBOX_TABLE_NAME);

        std::vector<std::string> keys;

        m_gearboxTable->getKeys(keys);

        for (auto& key: keys)
        {
            auto token = swss::tokenize(key, ':');

            /* "phy:<id>" is a PHY; "phy:<id>:ports:<n>" and friends are not. */
            if (token.size() != 2 || token[0] != "phy")
            {
                continue;
            }

            std::vector<swss::FieldValueTuple> values;

            m_gearboxTable->get(key, values);

            std::string hwinfo;
            std::string library;

            for (auto& fv: values)
            {
                if (fvField(fv) == "hwinfo")
                {
                    hwinfo = fvValue(fv);
                }
                else if (fvField(fv) == "phy_access_lib_name")
                {
                    library = fvValue(fv);
                }
            }

            if (hwinfo.empty() || library.empty())
            {
                continue;
            }

            m_libraryByHwinfo[hwinfo] = library;

            SWSS_LOG_INFO("gearbox PHY hwinfo '%s' uses MDIO access library %s",
                    hwinfo.c_str(), library.c_str());
        }
    }
    catch (const std::exception& e)
    {
        SWSS_LOG_ERROR("failed to read %s from APPL_DB: %s", GEARBOX_TABLE_NAME, e.what());

        m_libraryByHwinfo.clear();
    }

    SWSS_LOG_NOTICE("loaded MDIO access library names for %zu gearbox PHYs",
            m_libraryByHwinfo.size());
}

bool GearboxMdioAccess::getHardwareInfo(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t* attr_list,
        _Out_ std::string& hwinfo)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < attr_count; idx++)
    {
        if (attr_list[idx].id != SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO)
        {
            continue;
        }

        auto& s8list = attr_list[idx].value.s8list;

        if (s8list.list == NULL)
        {
            return false;
        }

        /* orchagent sends the string without its terminator. */
        hwinfo = std::string((const char*)s8list.list, s8list.count);

        return true;
    }

    return false;
}

const GearboxMdioAccess::MdioSymbols* GearboxMdioAccess::getSymbols(
        _In_ const std::string& hwinfo)
{
    SWSS_LOG_ENTER();

    if (!m_loaded)
    {
        loadLibraryNames();
    }

    auto configured = m_libraryByHwinfo.find(hwinfo);

    if (configured == m_libraryByHwinfo.end())
    {
        SWSS_LOG_ERROR("%s names no phy_access_lib_name for hwinfo '%s'",
                GEARBOX_TABLE_NAME, hwinfo.c_str());

        return nullptr;
    }

    const auto& path = configured->second;

    auto cached = m_symbolsByLibrary.find(path);

    if (cached != m_symbolsByLibrary.end())
    {
        return &cached->second;
    }

    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);

    if (handle == NULL)
    {
        SWSS_LOG_ERROR("failed to load MDIO access library %s: %s", path.c_str(), dlerror());

        return nullptr;
    }

    MdioSymbols symbols = {};

    dlerror();

    symbols.read = dlsym(handle, MDIO_READ_SYMBOL);
    symbols.write = dlsym(handle, MDIO_WRITE_SYMBOL);

    if (symbols.read == NULL || symbols.write == NULL)
    {
        SWSS_LOG_ERROR("MDIO access library %s is missing %s or %s: %s",
                path.c_str(), MDIO_READ_SYMBOL, MDIO_WRITE_SYMBOL, dlerror());

        dlclose(handle);

        return nullptr;
    }

    SWSS_LOG_NOTICE("loaded MDIO access library %s for hwinfo '%s'",
            path.c_str(), hwinfo.c_str());

    m_symbolsByLibrary[path] = symbols;

    return &m_symbolsByLibrary[path];
}

void GearboxMdioAccess::updateMdioPointers(
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t* attr_list)
{
    SWSS_LOG_ENTER();

    sai_attribute_t* readAttr = NULL;
    sai_attribute_t* writeAttr = NULL;

    for (uint32_t idx = 0; idx < attr_count; idx++)
    {
        switch (attr_list[idx].id)
        {
            case SAI_SWITCH_ATTR_REGISTER_READ:
                readAttr = &attr_list[idx];
                break;

            case SAI_SWITCH_ATTR_REGISTER_WRITE:
                writeAttr = &attr_list[idx];
                break;

            default:
                break;
        }
    }

    if (readAttr == NULL && writeAttr == NULL)
    {
        return;
    }

    std::string hwinfo;

    const MdioSymbols* symbols =
        getHardwareInfo(attr_count, attr_list, hwinfo) ? getSymbols(hwinfo) : nullptr;

    if (symbols == nullptr)
    {
        SWSS_LOG_ERROR("no MDIO access available for hwinfo '%s'; clearing the "
                "pointers so the vendor SAI rejects this switch create",
                hwinfo.c_str());
    }

    /* Null on failure beats the sender's address, which the vendor SAI would call. */
    if (readAttr != NULL)
    {
        readAttr->value.ptr = symbols ? symbols->read : NULL;
    }

    if (writeAttr != NULL)
    {
        writeAttr->value.ptr = symbols ? symbols->write : NULL;
    }
}
