"""Observed runtime display state. Never infer physical HDMI/DP encoding from VkFormat."""
import re

FORMATS = {37: ('RGBA8_UNORM', 8), 44: ('BGRA8_UNORM', 8),
           43: ('RGBA8_SRGB', 8), 50: ('BGRA8_SRGB', 8),
           58: ('A2RGB10_UNORM', 10), 64: ('A2BGR10_UNORM', 10)}
MODES = {0: 'immediate', 1: 'mailbox', 2: 'fifo', 3: 'fifo_relaxed'}
EFFECTS = {0: 'bilinear', 1: 'bilinear_dither', 2: 'cas_sharpen', 3: 'cas_sharpen_dither',
           4: 'cas_resample', 5: 'cas_resample_dither', 6: 'fsr_easu', 7: 'fsr_rcas', 8: 'fsr_rcas_dither'}


def summarize(text):
    result = {'surface_formats': [], 'present_modes': [], 'monitors': [], 'display_modes': [],
              'applied': None, 'output': None, 'pacing': [], 'fallbacks': [],
              'physical_link_encoding': 'system-controlled; not queried',
              'scope': 'observed runtime state; swap cadence is not gameplay speed or displayed FPS'}
    for line in text.splitlines():
        match = re.search(r'\[acv-display\] (\w+) (.*)', line)
        if not match:
            continue
        kind, payload = match.groups()
        fields = {}
        for key, value in re.findall(r'(\w+)=([^\s]+)', payload):
            try:
                fields[key] = float(value) if any(c in value for c in '.eE') else int(value)
            except ValueError:
                fields[key] = value
        key = {'surface_format': 'surface_formats', 'present_mode': 'present_modes',
               'monitor': 'monitors', 'display_mode': 'display_modes', 'fallback': 'fallbacks'}.get(kind)
        if key:
            if fields not in result[key]:
                result[key].append(fields)
        elif kind in ('applied', 'output'):
            result[kind] = fields
        elif kind == 'pacing':
            result['pacing'].append(fields)
            result['pacing'] = result['pacing'][-256:]
    if result['applied']:
        applied = result['applied']
        applied['format_name'], applied['bits'] = FORMATS.get(applied.get('format'), ('unknown', None))
        applied['present_mode'] = MODES.get(applied.get('mode'), 'unknown')
        applied['depth_fallback'] = applied['bits'] != applied.get('requested_bits')
    if result['output']:
        result['output']['final_effect_name'] = EFFECTS.get(result['output'].get('final_effect'), 'unknown')
    return result
