"""Test-only symbol locations in the current, verified embedded candidate."""
import json
from pathlib import Path
from capture_test_identity import root, data, addon, embedded, symbols, section
names = {'NR_FINAL_SUCCESS_RVA': 'g_successful_evaluations',
         'NR_TEST_TRANSFER_RVA': 'g_transfer_percent',
         'NR_TEST_COLOR_RVA': 'g_color_percent',
         'NR_TEST_SHARPNESS_RVA': 'g_sharpness_percent',
         'NR_TEST_PASSES_RVA': 'g_pass_controls',
         'NR_TEST_MEMORY_QUERY_RVA': 'query_local_memory',
         'NR_TEST_EFFECTIVE_SCALE_RVA': 'g_effective_scale',
         'NR_TEST_QUIESCE_RVA': 'g_quiesce_generation',
         'NR_FINAL_SCALED_RVA': 'g_scaled_calls',
         'NR_FINAL_FG_BYPASS_RVA': 'g_framegen_transparent_bypass',
         'NR_FINAL_FG_SCALED_RVA': 'g_framegen_scaled_calls',
         'NR_FINAL_PREWARM_RVA': 'g_prewarmed_sets',
         'NR_FINAL_RETIRED_RVA': 'g_retired_sets',
         'NR_FINAL_TRANSITION_NATIVE_RVA': 'g_transition_native_calls',
         'NR_FINAL_OFF_RVA': 'g_capture_off_until',
         'NR_FINAL_EVAL_RVA': 'scaled_evaluate_impl',
         'NR_TEST_SCALE_RVA': 'set_scale',
         'NR_TEST_TRACE_PUMP_RVA': 'pump_frame_trace',
         'NR_TEST_TRACE_REQUEST_RVA': 'g_trace_requested',
         'NR_TEST_TRACE_STATUS_RVA': 'g_trace_status'}
result = {}
for key, name in names.items():
    matches = [v for k, v in symbols.items() if k.startswith('?' + name + '@')]
    assert len(matches) == 1, (name, matches)
    rva = matches[0] + section[1] - 0x1000
    assert section[1] <= rva < section[1] + section[0] - 32
    result[key] = f'{rva:x}'
result['NR_FG_CALLBACK_RVA'] = f"{symbols['observed_framegen_callback'] + section[1] - 0x1000:x}"
print(json.dumps(result))
