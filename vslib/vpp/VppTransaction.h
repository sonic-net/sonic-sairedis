#pragma once

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace saivs
{
    /**
     * @brief Journal of undo actions for one SAI request.
     *
     * After each step of a request succeeds (a SaiVppXlate call or a change to
     * local state), the handler records the action that reverses it. If the
     * request fails, the actions run newest-first; if it succeeds, they are
     * dropped. An undo action returns 0 on success: a VPP return code or a
     * sai_status_t.
     *
     * Conventions for undo actions:
     * - Capture arguments by value. Callers reuse stack structs between calls,
     *   so a captured reference can name the wrong object by the time the
     *   action runs.
     * - For a call that returns an index or handle, make the call, then
     *   record the undo with the returned value.
     * - For a set, keep the previous value and restore it.
     * - Undoing a remove re-creates what it deleted, from arguments rebuilt
     *   from local state. Record nothing for a step that cannot be rebuilt.
     * - Deferred work a request triggers is part of it: its undo restores
     *   both the VPP state and the deferred bookkeeping.
     * - Record from the thread that owns the request. The active transaction
     *   is per thread.
     */
    class VppTransaction
    {
        public:

            using Undo = std::function<int()>;

            VppTransaction() = default;

            VppTransaction(const VppTransaction&) = delete;
            VppTransaction& operator=(const VppTransaction&) = delete;

            /**
             * @brief Active transaction of the calling thread.
             *
             * With no TxnScope open on the thread, this is an inactive
             * transaction that records nothing.
             */
            static VppTransaction& current();

            bool active() const;

            /**
             * @brief Record an undo for a step that already succeeded.
             */
            void onRollback(
                    _In_ const std::string& desc,
                    _In_ Undo undo);

            /**
             * @brief Run a step, and record its undo if it returns 0.
             *
             * @return The step's return value.
             */
            template <class Do, class UndoFn>
            int run(
                    _In_ const std::string& desc,
                    _In_ Do&& step,
                    _In_ UndoFn&& undo)
            {
                SWSS_LOG_ENTER();

                int ret = step();

                if (ret == 0)
                {
                    onRollback(desc, std::forward<UndoFn>(undo));
                }

                return ret;
            }

            size_t savepoint() const;

            /**
             * @brief Run the undo actions recorded after a savepoint, newest
             * first. A failing action does not stop the others.
             *
             * @return SAI_STATUS_FAILURE if any undo action failed.
             */
            sai_status_t rollbackTo(
                    _In_ size_t savepoint);

            sai_status_t rollback();

            void commit();

        private:

            friend class TxnScope;

            struct Entry
            {
                std::string desc;
                Undo undo;
            };

            std::vector<Entry> m_journal;

            bool m_active = false;

            bool m_rollingBack = false;
    };

    /**
     * @brief Scope of a transaction on the calling thread.
     *
     * The outermost scope on a thread owns the transaction. A scope opened
     * inside another one is a savepoint of it: committing keeps its undo
     * actions for the enclosing scope, rolling back runs only its own. A
     * scope left without commit() rolls back.
     *
     * A scope opened by an undo action, while its transaction rolls back,
     * owns a transaction of its own, so a helper the undo calls still undoes
     * its partial work if it fails.
     */
    class TxnScope
    {
        public:

            TxnScope();

            ~TxnScope();

            TxnScope(const TxnScope&) = delete;
            TxnScope& operator=(const TxnScope&) = delete;

            VppTransaction& txn();

            void commit();

            /**
             * @brief Run the undo actions recorded since this scope opened.
             *
             * @return SAI_STATUS_FAILURE if any undo action failed.
             */
            sai_status_t rollback();

        private:

            VppTransaction m_root;

            VppTransaction* m_txn;

            VppTransaction* m_previous;

            size_t m_savepoint;

            bool m_done = false;
    };
}
