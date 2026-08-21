# Fix: orchagent ZMQ to syncd gets stuck on shutdown

**PR Description**: Add `SaiInterface::shutdown()` API to propagate SIGTERM to ZeroMQChannel, breaking out of zmq_poll EINTR retry loop immediately instead of blocking up to 600s during shutdown.

## Issue

- **GitHub Issue**: [sonic-net/sonic-buildimage#27910](https://github.com/sonic-net/sonic-buildimage/issues/27910)
- **Related PR**: [sonic-net/sonic-sairedis#1881](https://github.com/sonic-net/sonic-sairedis/pull/1881) (fixed the crash, not the hang)

## Problem

During `config reload -y -f`, syncd shuts down before swss. When orchagent's
periodic timers (CrmOrch, PfcWdOrch, etc.) issue SAI calls over ZMQ to a dead
syncd, `zmq_poll()` blocks for 60s. If SIGTERM arrives during the poll, EINTR
is returned but the code retries up to 10 times — potentially blocking for
600s. Since the `gOrchShutdownRequested` flag is only checked in orchagent's
main `select()` loop, the process cannot exit.

### Timeline (from issue)

```
05:30:41  syncd shuts down, ZMQ channel torn down
05:30:43  CRM timer fires → enters zmq_poll() → blocks
05:30:59  swss gets SIGTERM → sets gOrchShutdownRequested
05:31:00  zmq_poll gets EINTR → retries → blocks again
05:32:00  zmq_poll times out (60s) → ZMQ_EFSM on next send → crash/abort
```

### Prior fix (PR #1881)

Fixed the crash after timeout (ZMQ_EFSM on `zmq_send`). Did NOT fix:
1. The 60s hang during `zmq_poll` blocking shutdown
2. ERROR log spam from timeout (fails ASAN error-checking)

## Solution

### Design

Add a `shutdown()` method to `SaiInterface` that propagates a shutdown flag down
to the `Channel` layer. On EINTR, `ZeroMQChannel::wait()` checks this flag and
returns `SAI_STATUS_FAILURE` immediately instead of retrying.

### Architecture

```
orchagent signal handler (sonic-swss)
  → gSai->shutdown()                          // SaiInterface virtual method
    → Sai::shutdown()
      → context->m_redisSai->shutdown()        // RedisRemoteSaiInterface
        → m_communicationChannel->setShutdown() // Channel base class
          → m_shutdown = true                   // volatile bool

ZeroMQChannel::wait() [blocked in zmq_poll]
  → zmq_poll returns -1, errno == EINTR
  → checks m_shutdown == true
  → returns SAI_STATUS_FAILURE immediately     // orchagent unblocks
```

### sonic-sairedis changes

#### Channel.h / Channel.cpp — Shutdown flag on base class

```cpp
// Channel.h — new public methods
void setShutdown();
bool isShutdown() const;

// Channel.h — new protected member
volatile bool m_shutdown;

// Channel.cpp — implementation
Channel::Channel(_In_ Callback callback):
    m_callback(callback),
    m_responseTimeoutMs(SAI_REDIS_DEFAULT_SYNC_OPERATION_RESPONSE_TIMEOUT),
    m_shutdown(false)
{ }

void Channel::setShutdown()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("setting channel shutdown flag");
    m_shutdown = true;
}

bool Channel::isShutdown() const
{
    return m_shutdown;
}
```

#### ZeroMQChannel.cpp — Check shutdown on EINTR, downgrade timeout log

```cpp
for (int i = 0; true ; ++i)
{
    rc = zmq_poll(items, 1, (int)m_responseTimeoutMs);

    if (rc == 0)
    {
        // Downgraded from ERROR to WARN — timeout when syncd is down
        // during shutdown is expected, not an error
        SWSS_LOG_WARN("zmq_poll timed out for: %s", command.c_str());
        return SAI_STATUS_FAILURE;
    }
    if (rc < 0 && zmq_errno() == EINTR)
    {
        if (m_shutdown)
        {
            SWSS_LOG_NOTICE("zmq_poll interrupted during shutdown for: %s",
                    command.c_str());
            return SAI_STATUS_FAILURE;
        }

        if (i < ZMQ_MAX_RETRY)
        {
            continue;  // spurious signal (SIGUSR1 log rotation) — retry
        }
    }
    if (rc < 0)
    {
        SWSS_LOG_THROW("zmq_poll failed, zmqerrno: %d", zmq_errno());
    }
    break;
}
```

#### SaiInterface.h — Virtual shutdown method (non-breaking)

```cpp
public: // non SAI API

    virtual sai_log_level_t logGet(_In_ sai_api_t api);

    /**
     * @brief Signal that the caller is shutting down.
     *
     * Allows blocking operations (e.g. zmq_poll) to be interrupted promptly
     * instead of waiting for full timeout. Safe to call from signal handler.
     */
    virtual void shutdown() { /* default: no-op */ }
```

#### Concrete implementations

| Class | Implementation |
|-------|---------------|
| `Sai` | Iterates `m_contextMap`, calls `m_redisSai->shutdown()` on each |
| `ClientSai` | Calls `m_communicationChannel->setShutdown()` |
| `ClientServerSai` | Delegates to `m_sai->shutdown()` |
| `RedisRemoteSaiInterface` | Calls `m_communicationChannel->setShutdown()` |

### sonic-swss changes (companion PR)

In orchagent's signal handler, after setting `gOrchShutdownRequested`, call
`shutdown()` on the SAI interface to immediately unblock any in-progress
ZMQ poll:

```cpp
// orchagent/main.cpp — signal handler
void sigterm_handler(int signo)
{
    gOrchShutdownRequested = signo;

    // Unblock any in-progress blocking SAI calls (zmq_poll)
    if (gSai)
        gSai->shutdown();
}
```

This is signal-safe because `shutdown()` only writes a `volatile bool`.

**Note**: The exact variable name and location depends on how orchagent
exposes its `SaiInterface*`. The pointer may be accessed via a global or
through the orchestrator framework. The key requirement is calling
`saiInterface->shutdown()` from the SIGTERM handler.

## Behavior After Fix

| Scenario | Before | After |
|----------|--------|-------|
| SIGTERM during zmq_poll | Retries up to 10x (600s worst case) | Returns SAI_STATUS_FAILURE immediately |
| Spurious SIGUSR1 during zmq_poll | Retries (correct) | Retries (unchanged — m_shutdown is false) |
| zmq_poll timeout (syncd down) | Logs ERROR | Logs WARN |
| zmq_send / zmq_recv EINTR | Retries up to 10x (fast ops) | Unchanged — retry is correct for these |

## Why Not Simpler Alternatives?

| Alternative | Problem |
|-------------|---------|
| Don't retry zmq_poll on EINTR at all | Spurious signals (SIGUSR1 for log rotation) would cause false SAI_STATUS_FAILURE for non-CRM callers (RouteOrch, AclOrch) during normal operation |
| Reduce retry count to 2 | Still blocks up to 120s — exceeds some Docker timeouts; doesn't solve the fundamental "can't distinguish SIGTERM from spurious signal" problem |
| Fix only in CrmOrch (sonic-swss) | Only covers one caller. Other orchs (PfcWdOrch, FabricOrch, WatermarkOrch, BufferOrch, NatOrch) have the same issue. Also can't guard against signals arriving mid-call. |
| Check gOrchShutdownRequested in ZeroMQChannel | Cross-layer violation — sonic-sairedis shouldn't know about sonic-swss globals |

## Testing

1. **Unit test**: Mock a signal delivery during `ZeroMQChannel::wait()`, verify
   that with `setShutdown()` called, the function returns immediately.
2. **VS integration test**: Start orchagent in ZMQ mode against vslib, kill
   syncd, send SIGTERM to orchagent, verify it exits within seconds.
3. **ASAN build**: Run `config reload -y -f` — verify no ERROR logs from
   zmq_poll timeout and no crash/hang.
