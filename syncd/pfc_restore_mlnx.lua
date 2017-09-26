-- KEYS - queue IDs
-- ARGV[1] - counters db index
-- ARGV[2] - counters table name
-- ARGV[3] - poll time interval
-- return queue Ids that satisfy criteria

local counters_db = ARGV[1]
local counters_table_name = ARGV[2]
local poll_time = tonumber(ARGV[3])

local rets = {}

redis.call('SELECT', counters_db)

-- Iterate through each queue
local n = table.getn(KEYS)
for i = n, 1, -1 do
    local counter_keys = redis.call('HKEYS', counters_table_name .. ':' .. KEYS[i])
    local pfc_rx_pkt_key = ''
    local pfc_wd_status = redis.call('HGET', counters_table_name .. ':' .. KEYS[i], 'PFC_WD_STATUS')
    if pfc_wd_status ~= 'operational' then
        local restoration_time = tonumber(redis.call('HGET', counters_table_name .. ':' .. KEYS[i], 'PFC_WD_RESTORATION_TIME'))
        local time_left = redis.call('HGET', counters_table_name .. ':' .. KEYS[i], 'PFC_WD_RESTORATION_TIME_LEFT')
        if time_left == nil then
            time_left = restoration_time
        else
            time_left = tonumber(time_left)
        end

        local m = table.getn(counter_keys)
        for j = m, 1, -1 do
            if string.sub(counter_keys[j],-string.len('RX_PKTS'))=='RX_PKTS' then
                pfc_rx_pkt_key = counter_keys[j]
            end
        end

        if pfc_rx_pkt_key ~= '' then
            local pfc_rx_packets = tonumber(redis.call('HGET', counters_table_name .. ':' .. KEYS[i], pfc_rx_pkt_key))
            local pfc_rx_packets_last = redis.call('HGET', counters_table_name .. ':' .. KEYS[i], pfc_rx_pkt_key .. '_last')
            -- DEBUG CODE START. Uncomment to enable
            local debug_storm = redis.call('HGET', counters_table_name .. ':' .. KEYS[i], 'DEBUG_STORM')
            -- DEBUG CODE END.
            if not pfc_rx_packets_last then
                pfc_rx_packets_last = pfc_rx_packets
            else
                pfc_rx_packets_last = tonumber(pfc_rx_packets_last)

                -- Check actual condition of queue being restored from PFC storm
                if (pfc_rx_packets - pfc_rx_packets_last == 0)
                    -- DEBUG CODE START. Uncomment to enable
                    and (debug_storm ~= "enabled")
                    -- DEBUG CODE END.
                    then
                        if time_left < poll_time then
                            table.insert(rets, KEYS[i])
                            time_left = restoration_time
                        else
                            time_left = time_left - poll_time
                        end
                else
                    time_left = restoration_time
                end
            end

            -- Save values for next run
            redis.call('HSET', counters_table_name .. ':' .. KEYS[i], 'PFC_WD_RESTORATION_TIME_LEFT', time_left)
            redis.call('HSET', counters_table_name .. ':' .. KEYS[i], pfc_rx_pkt_key .. '_last', pfc_rx_packets)
        end
    end
end

return rets
