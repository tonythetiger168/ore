# ila_mining.tcl -- ILA template. Run after synthesis with MARK_DEBUG set.
# Common bring-up triggers: engine never asserts found (check tagNonce
# alignment shift registers), found but wrong nonce (endianness), busy stuck
# (template load FSM).
create_debug_core ila_eng ila
set_property C_DATA_DEPTH 4096 [get_debug_cores ila_eng]
set_property C_TRIGIN_EN false [get_debug_cores ila_eng]
# probe0: {eng0_found, eng0_busy, eng0_done, eng0_state[1:0]}
# probe1: eng0_nonce[31:0]
# probe2: eng0_foundNonce[31:0]
# probe3: eng0_tagNonce_pipe_valid (alignment sanity)
connect_debug_port ila_eng/clk [get_nets eng_clk]
# ... connect probes per MARK_DEBUG nets, then implement_debug
