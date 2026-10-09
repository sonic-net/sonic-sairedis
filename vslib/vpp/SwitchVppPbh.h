/*
 *------------------------------------------------------------------
 * SwitchVppPbh.h
 *
 * Policy Based Hashing (SAI PBH) for the VPP switch.
 *
 * Copyright (c) 2026 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *------------------------------------------------------------------
 */

#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "SwitchVpp.h"
#include "SaiObjectDB.h"

/*
 * SwitchVpp.h includes this header before it reaches its own includes, so
 * anything named below has to be pulled in here rather than inherited.
 */
#include "vppxlate/SaiVppXlate.h"

namespace saivs
{
    class SwitchVpp;

    /**
     * @brief Policy Based Hashing state and operations.
     *
     * PbhOrch models PBH as an ACL table of its own type plus a set of
     * SAI_OBJECT_TYPE_HASH profiles referenced by the entries' SET_ECMP_HASH_ID
     * and SET_LAG_HASH_ID actions. None of it reaches the VPP ACL plugin: the
     * plugin can neither follow the variable offsets PBH qualifies on nor steer
     * a packet to a chosen load-balance bucket. The sonic_ext plugin's PBH
     * feature does both, so PBH keeps its own index space, its own counters and
     * its own interface attachments, all held here rather than alongside the
     * ACL plugin's state in SwitchVpp.
     */
    class SwitchVppPbh
    {
        public:

            SwitchVppPbh(
                    _In_ SwitchVpp *switch_db);

            /**
             * @brief Whether an ACL table is a PBH table.
             *
             * @param tbl_oid The ACL table object ID.
             */
            bool isPbhTable(
                    _In_ sai_object_id_t tbl_oid);

            /**
             * @brief Whether an ACL counter belongs to a PBH rule.
             *
             * @param cntr_oid The ACL counter object ID.
             */
            bool isPbhCounterOid(
                    _In_ sai_object_id_t cntr_oid);

            /**
             * @brief Create a hash object, programming a PBH profile when it is
             * a fine grained hash.
             *
             * @param object_id The hash object ID.
             * @param switch_id The switch ID.
             * @param attr_count The number of attributes in the attribute list.
             * @param attr_list The attribute list.
             * @return The status of the operation.
             */
            sai_status_t createHash(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            /**
             * @brief Update the PBH profile behind a fine grained hash.
             *
             * Only SAI_HASH_ATTR_FINE_GRAINED_HASH_FIELD_LIST changes what VPP
             * hashes on; every other attribute, and every hash that is not a
             * PBH profile, is stored and otherwise left alone.
             *
             * @param object_id The hash object ID.
             * @param attr The attribute being set.
             * @return The status of the operation.
             */
            sai_status_t setHash(
                    _In_ sai_object_id_t object_id,
                    _In_ const sai_attribute_t *attr);

            /**
             * @brief Remove a hash object and its PBH profile, if it had one.
             *
             * @param serializedObjectId The serialized hash object ID.
             * @return The status of the operation.
             */
            sai_status_t removeHash(
                    _In_ const std::string &serializedObjectId);

            /**
             * @brief Create an empty PBH table in VPP for an ACL table.
             *
             * @param tbl_oid The ACL table object ID.
             * @return The status of the operation.
             */
            sai_status_t tableCreate(
                    _In_ sai_object_id_t tbl_oid);

            /**
             * @brief Remove the PBH table backing an ACL table.
             *
             * @param tbl_oid The ACL table object ID.
             * @return The status of the operation.
             */
            sai_status_t tableRemove(
                    _In_ sai_object_id_t tbl_oid);

            /**
             * @brief Program the PBH table's whole rule set from the ACL
             * entries currently listed under it.
             *
             * @param tbl_oid The ACL table object ID.
             * @return The status of the operation.
             */
            sai_status_t tableConfig(
                    _In_ sai_object_id_t tbl_oid);

            /**
             * @brief Attach or detach a PBH table to a VPP interface.
             *
             * @param hwif_name The VPP interface name.
             * @param tbl_oid The ACL table object ID.
             * @param is_bind True to attach, false to detach.
             * @return The status of the operation.
             */
            sai_status_t bindUnbindPort(
                    _In_ const std::string &hwif_name,
                    _In_ sai_object_id_t tbl_oid,
                    _In_ bool is_bind);

            /**
             * @brief Read the packet and byte counters of a PBH rule.
             *
             * @param cntr_oid The ACL counter object ID.
             * @param attr_count The number of attributes in the attribute list.
             * @param attr_list The attribute list.
             * @return The status of the operation.
             */
            sai_status_t getEntryStats(
                    _In_ sai_object_id_t cntr_oid,
                    _In_ uint32_t attr_count,
                    _Out_ sai_attribute_t *attr_list);

        private:

            /**
             * @brief Where a PBH rule's counters live in VPP.
             *
             * Unlike an ACE, one PBH SAI entry is exactly one VPP rule, so this
             * carries a bare position rather than a {base, count} span.
             */
            typedef struct pbh_cntr_info_
            {
                sai_object_id_t tbl_oid;
                sai_object_id_t entry_oid;
                uint32_t table_index;
                uint32_t rule_index;

            } pbh_cntr_info_t;

            /**
             * @brief Whether the sonic_ext PBH feature is enabled in VPP,
             * logging the first object refused because it is not.
             *
             * @param what Kind of object being refused, for the log line.
             * @param sid Serialized object ID being refused.
             */
            bool pbhSupported(
                    _In_ const char *what,
                    _In_ const std::string &sid);

            /**
             * @brief Translate a fine grained hash into the VPP field vector.
             *
             * @param hash_obj The hash object, read through whichever view the
             *                 caller has: the pending attribute list on create,
             *                 the stored object overlaid with the set on update.
             * @param sid Serialized hash object ID, for log lines.
             * @param fields Receives the translated fields.
             * @return SAI_STATUS_ITEM_NOT_FOUND if the hash is not fine grained
             *         and so is not a PBH profile at all, SAI_STATUS_SUCCESS on
             *         success, an error otherwise.
             */
            sai_status_t hashProfileFields(
                    _In_ const SaiObject &hash_obj,
                    _In_ const std::string &sid,
                    _Out_ std::vector<vpp_pbh_hash_field_t> &fields);

            /**
             * @brief Drop every counter recorded for a table.
             *
             * @param tbl_oid The ACL table object ID.
             */
            void forgetTableCounters(
                    _In_ sai_object_id_t tbl_oid);

            SwitchVpp *m_switch_db;

            // ACL table OIDs recognised as PBH tables.
            std::set<sai_object_id_t> m_tables;

            // ACL table OID -> VPP PBH table index.
            std::map<sai_object_id_t, uint32_t> m_table_index_map;

            // SAI_OBJECT_TYPE_HASH OID -> VPP PBH profile index.
            std::map<sai_object_id_t, uint32_t> m_profile_map;

            // ACL counter OID -> the VPP rule it counts.
            std::map<sai_object_id_t, pbh_cntr_info_t> m_cntr_info_map;
    };
}
