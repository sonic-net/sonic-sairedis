#pragma once

#include "sai.h"

#include <algorithm>
#include <set>
#include <vector>

namespace saivs
{
    struct VppAclInterfacePolicy
    {
        std::set<sai_object_id_t> ingress;
        std::set<sai_object_id_t> egress;
        std::vector<sai_object_id_t> ingressOrder;
        std::vector<sai_object_id_t> egressOrder;
        bool ingressDefault = false;
        bool egressDefault = false;
        bool ingressDefaultPending = false;
        bool egressDefaultPending = false;

        struct Binding
        {
            sai_object_id_t table;
            bool is_input;
        };

        void updateTable(sai_object_id_t table, bool is_input, bool is_bind)
        {
            auto &tables = is_input ? ingress : egress;
            auto &order = is_input ? ingressOrder : egressOrder;
            if (is_bind)
            {
                if (tables.insert(table).second)
                {
                    order.push_back(table);
                }
            }
            else
            {
                tables.erase(table);
                order.erase(std::remove(order.begin(), order.end(), table), order.end());
            }
        }

        bool empty() const
        {
            return ingress.empty() && egress.empty() && !ingressDefault &&
                !egressDefault && !ingressDefaultPending && !egressDefaultPending;
        }

        // Null table denotes the shared permit, always last in its direction.
        std::vector<Binding> bindingPlan() const
        {
            std::vector<Binding> plan;
            for (bool is_input : {true, false})
            {
                const auto &order = is_input ? ingressOrder : egressOrder;
                for (auto table : order)
                {
                    plan.push_back({table, is_input});
                }
                if (is_input ? (ingressDefault || ingressDefaultPending) :
                               (egressDefault || egressDefaultPending))
                {
                    plan.push_back({SAI_NULL_OBJECT_ID, is_input});
                }
            }
            return plan;
        }
    };
}
