"""Opt-in LAN integration: python tests/mdns-network.py EXE IPv4 INTERFACE_INDEX.
Uses only unique test hostnames. Never changes DNS, interfaces or firewall rules.
Requires the Qt runtime on PATH. Includes real queries, a goodbye and a duplicate-name test.
"""
import ipaddress
import queue
import socket
import struct
import subprocess
import sys
import threading
import time
import uuid

exe, local_ip, interface = sys.argv[1:4]
group = ('224.0.0.251', 5353)


def wire_name(name):
    return b''.join(bytes([len(label)]) + label.encode('ascii') for label in name.split('.')) + b'\0'


def read_name(packet, position):
    labels, seen, after = [], set(), None
    while True:
        if position in seen:
            raise ValueError('compression cycle')
        seen.add(position)
        size = packet[position]
        position += 1
        if size & 0xc0 == 0xc0:
            after = after or position + 1
            position = ((size & 0x3f) << 8) | packet[position]
        elif size:
            labels.append(packet[position:position + size].decode('ascii'))
            position += size
        else:
            return '.'.join(labels), after or position


def records(packet):
    ident, flags, questions, answers, authority, extra = struct.unpack('!6H', packet[:12])
    position = 12
    for _ in range(questions):
        _, position = read_name(packet, position)
        position += 4
    result = []
    for _ in range(answers + authority + extra):
        name, position = read_name(packet, position)
        kind, klass, ttl, size = struct.unpack_from('!HHIH', packet, position)
        position += 10
        result.append((name, kind, klass, ttl, packet[position:position + size]))
        position += size
    return ident, flags, result


def udp(shared=False):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(local_ip))
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
    sock.settimeout(.2)
    sock.bind(('', 5353 if shared else 0))
    if shared:
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                        socket.inet_aton(group[0]) + socket.inet_aton(local_ip))
    return sock


def publisher(label, seconds):
    process = subprocess.Popen([exe, '--serve', label, interface, str(seconds)],
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, encoding='utf-8', errors='replace')
    output = queue.Queue()
    def collect():
        for line in process.stdout:
            output.put(line.strip())
    threading.Thread(target=collect, daemon=True).start()
    return process, output


def wait_line(output, token, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            line = output.get(timeout=.2)
        except queue.Empty:
            continue
        if token in line:
            return line
    raise AssertionError('publisher did not report ' + token)


def query(name, kind=1):
    ident = 1234
    packet = struct.pack('!6H', ident, 0, 1, 0, 0, 0) + wire_name(name) + struct.pack('!HH', kind, 1)
    with udp() as client:
        client.sendto(packet, group)
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            try:
                data, _ = client.recvfrom(9000)
            except socket.timeout:
                continue
            parsed_id, flags, result = records(data)
            if parsed_id == ident and flags & 0x8000:
                return result
    raise AssertionError('no legacy unicast response')


label = 'obs-test-' + uuid.uuid4().hex[:10]
name = label + '.local'
with udp(shared=True) as monitor:
    process, output = publisher(label, 8)
    try:
        wait_line(output, 'mDNS anunciado:')
        response = query(name)
        assert any(n == name and kind == 1 and ttl == 10 and data == socket.inet_aton(local_ip)
                   for n, kind, _, ttl, data in response), response
        response = query(name, 28)
        assert any(n == name and kind == 47 for n, kind, *_ in response), response
        goodbye = False
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            try:
                data, _ = monitor.recvfrom(9000)
            except socket.timeout:
                continue
            _, flags, entries = records(data)
            if flags & 0x8000 and any(n == name and kind == 1 and ttl == 0 for n, kind, _, ttl, _ in entries):
                goodbye = True
                break
        assert goodbye, 'no goodbye observed on multicast'
        assert process.wait(timeout=3) == 0
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=3)

label = 'obs-conflict-' + uuid.uuid4().hex[:8]
name = label + '.local'
process, output = publisher(label, 8)
try:
    wait_line(output, 'mDNS anunciado:')
    fake_ip = str(ipaddress.IPv4Address(int(ipaddress.IPv4Address(local_ip)) ^ 1))
    conflict = (struct.pack('!6H', 0, 0x8400, 0, 1, 0, 0) + wire_name(name)
                + struct.pack('!HHIH', 1, 0x8001, 120, 4) + socket.inet_aton(fake_ip))
    with udp(shared=True) as sender:
        sender.sendto(conflict, group)
        time.sleep(.15)
        sender.sendto(conflict, group)
    wait_line(output, 'ya est')
    assert process.wait(timeout=10) == 0
finally:
    if process.poll() is None:
        process.terminate()
        process.wait(timeout=3)
print('PASS: real mDNS A response, AAAA denial, goodbye on stop, duplicate hostname withdraws')
