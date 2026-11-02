"""Shared USB device selection for clock tools."""

def select_port(requested):
    from serial.tools.list_ports import comports
    if requested:
        return requested
    ports = [p for p in comports() if p.vid == 0x303A]
    if not ports:
        raise ValueError('No Espressif USB device found. Enter maintenance mode, or use --port PORT.')
    if len(ports) == 1:
        return ports[0].device
    for i, port in enumerate(ports, 1):
        print(f'{i}: {port.device} ({port.description})')
    choice = int(input('Select device number: '))
    if not 1 <= choice <= len(ports):
        raise ValueError('Invalid device number.')
    return ports[choice - 1].device


