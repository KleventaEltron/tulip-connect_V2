"""Bench-only S2 Connect + WebSocket CEM. Does not control a heat pump."""
import argparse
import base64
import datetime as dt
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import secrets
import socket
import ssl
import struct
import threading
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from jsonschema.exceptions import ValidationError

ROOT = Path(__file__).resolve().parents[2]
VERSION = 'v1.0.0'


def token():
    return base64.b64encode(secrets.token_bytes(32)).decode('ascii')


def validators():
    from jsonschema import Draft202012Validator, FormatChecker
    directory = ROOT / 'tests/s2_messages/spec'
    def expand(obj, source):
        if isinstance(obj, list):
            return [expand(x, source) for x in obj]
        if not isinstance(obj, dict):
            return obj
        if '$ref' in obj:
            target = (source.parent / obj['$ref']).resolve()
            if not target.is_relative_to(directory.resolve()):
                raise ValueError('nonlocal schema reference')
            return expand(json.loads(target.read_text()), target)
        return {k: expand(v, source) for k, v in obj.items()}
    result = {}
    for source in (directory / 'messages').glob('*.json'):
        schema = expand(json.loads(source.read_text()), source)
        result[schema['title']] = Draft202012Validator(schema, format_checker=FormatChecker())
    return result


def initialize(directory, ip, hostname, port, header_path=None):
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID
    ipaddress.IPv4Address(ip)
    if not hostname or len(hostname) >= 64 or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-.' for c in hostname):
        raise ValueError('use a lowercase DNS hostname shorter than 64 characters')
    if not 1 <= port <= 65535:
        raise ValueError('invalid port')
    directory.mkdir(parents=True, exist_ok=True)
    header = header_path or ROOT / 'src/files/s2/s2_comm_test_local.h'
    if (directory / 'config.json').exists() or header.exists():
        raise ValueError('existing lab identity/configuration: reuse it; init will not overwrite credentials')
    now = dt.datetime.now(dt.timezone.utc)
    root_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    root_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'Tulip S2 bench CA')])
    root = (x509.CertificateBuilder().subject_name(root_name).issuer_name(root_name)
        .public_key(root_key.public_key()).serial_number(x509.random_serial_number())
        .not_valid_before(now-dt.timedelta(minutes=5)).not_valid_after(now+dt.timedelta(days=30))
        .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
        .add_extension(x509.KeyUsage(digital_signature=True, key_encipherment=False, content_commitment=False,
            data_encipherment=False, key_agreement=False, key_cert_sign=True, crl_sign=True,
            encipher_only=None, decipher_only=None), critical=True).sign(root_key, hashes.SHA256()))
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, hostname)])
    cert = (x509.CertificateBuilder().subject_name(name).issuer_name(root_name)
        .public_key(key.public_key()).serial_number(x509.random_serial_number())
        .not_valid_before(now-dt.timedelta(minutes=5)).not_valid_after(now+dt.timedelta(days=30))
        .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
        .add_extension(x509.SubjectAlternativeName([x509.DNSName(hostname)]), critical=False)
        .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
        .sign(root_key, hashes.SHA256()))
    config = dict(ip=ip, hostname=hostname, port=port, client=str(uuid.uuid4()), server=str(uuid.uuid4()), initial_token=token())
    (directory / 'config.json').write_text(json.dumps(config, indent=2)+'\n')
    (directory / 'server-key.pem').write_bytes(key.private_bytes(serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    (directory / 'server.pem').write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    (directory / 'ca.pem').write_bytes(root.public_bytes(serialization.Encoding.PEM))
    der = root.public_bytes(serialization.Encoding.DER)
    binding = secrets.token_bytes(32)
    array = lambda data: ','.join('0x%02x' % b for b in data)
    header.write_text('/* Generated private bench fixture; do not commit. */\n'
        '#define S2_COMM_TEST_API "https://%s:%d/v1"\n' % (hostname, port) +
        '#define S2_COMM_TEST_CLIENT "%s"\n#define S2_COMM_TEST_SERVER "%s"\n' % (config['client'], config['server']) +
        '#define S2_COMM_TEST_INITIAL_TOKEN "%s"\n' % config['initial_token'] +
        '#define S2_COMM_TEST_IPV4 0x%08xu\n' % struct.unpack('<I', ipaddress.IPv4Address(ip).packed)[0] +
        'static const uint8_t s2_comm_test_binding[32]={%s};\n' % array(binding) +
        'static const uint8_t s2_comm_test_ca[]={%s};\n' % array(der))
    print('Created private bench identity, TLS certificates and firmware header. Test enable remains OFF.')


class State:
    def __init__(self, directory):
        self.directory = directory
        self.config = json.loads((directory/'config.json').read_text())
        self.lock = threading.RLock()
        self.path = directory/'state.json'
        self.data = json.loads(self.path.read_text()) if self.path.exists() else {'active': self.config['initial_token'], 'pending': None, 'pending_at': 0}
        self.communications = {}
        self.check = validators()
        self.passed = threading.Event()
    def save(self):
        temporary = self.path.with_suffix('.new')
        with temporary.open('w') as stream:
            json.dump(self.data, stream)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, self.path)
    def initiate(self, bearer, request):
        with self.lock:
            if not secrets.compare_digest(bearer, self.data['active']):
                return 401, {}
            if request.get('clientNodeId') != self.config['client'] or request.get('serverNodeId') != self.config['server']:
                return 403, {'errorMessage': 'NoLongerPaired'}
            if VERSION not in request.get('supportedS2MessageVersions', []) or 'WebSocket' not in request.get('supportedCommunicationProtocols', []):
                return 400, {'errorMessage': 'UnsupportedVersion'}
            self.data.update(pending=token(), pending_at=time.time())
            self.save()
            return 200, dict(selectedCommunicationProtocol='WebSocket', selectedS2MessageVersion=VERSION, accessToken=self.data['pending'])
    def confirm(self, bearer):
        with self.lock:
            if not self.data['pending'] or time.time()-self.data['pending_at'] >= 15 or not secrets.compare_digest(bearer, self.data['pending']):
                return 401, {}
            self.data.update(active=self.data['pending'], pending=None, pending_at=0)
            self.save()
            communication = token()
            self.communications = {t: until for t, until in self.communications.items() if until > time.monotonic()}
            self.communications[communication] = time.monotonic()+30
            return 200, dict(communicationProtocol='WebSocket', websocketToken=communication,
                websocketUrl='wss://%s:%d/s2' % (self.config['hostname'], self.config['port']))


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def log_message(self, *args):
        pass  # No token-bearing requests or headers in console logs.
    @property
    def state(self):
        return self.server.state
    def bearer(self):
        value = self.headers.get('Authorization', '')
        return value[7:] if value.startswith('Bearer ') else ''
    def reply(self, code, body):
        data = json.dumps(body, separators=(',', ':')).encode()
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)
    def do_POST(self):
        try:
            self.connection.settimeout(10)
            length = int(self.headers.get('Content-Length', '-1'))
            if not 0 <= length <= 4096 or self.headers.get('Transfer-Encoding'):
                self.close_connection = True
                return self.reply(400, {})
            raw = self.rfile.read(length)
            request = json.loads(raw) if raw else {}
            if not isinstance(request, dict):
                return self.reply(400, {})
            if self.path == '/v1/initiateSession':
                code, body = self.state.initiate(self.bearer(), request)
            elif self.path == '/v1/confirmAccessToken':
                code, body = self.state.confirm(self.bearer())
            else:
                code, body = 404, {}
            self.reply(code, body)
        except (ValueError, TimeoutError):
            self.close_connection = True
            self.reply(400, {})
    def frame(self, opcode, data):
        head = bytes([0x80 | opcode])
        head += bytes([len(data)]) if len(data) < 126 else b'\x7e'+struct.pack('!H', len(data))
        self.wfile.write(head+data)
        self.wfile.flush()
    def read_exact(self, count):
        data = self.rfile.read(count)
        if len(data) != count:
            raise ConnectionError('peer closed')
        return data
    def read_message(self):
        content = bytearray()
        fragmented = False
        while True:
            a, b = self.read_exact(2)
            fin, op = bool(a & 128), a & 15
            if a & 112 or not b & 128:
                raise ValueError('invalid client frame')
            length = b & 127
            if length == 126:
                length = struct.unpack('!H', self.read_exact(2))[0]
            elif length == 127:
                raise ValueError('oversized frame')
            if length > 4096 or (op >= 8 and (not fin or length > 125)):
                raise ValueError('frame limit')
            mask = self.read_exact(4)
            data = bytes(v ^ mask[i % 4] for i, v in enumerate(self.read_exact(length)))
            if op == 8:
                self.frame(8, data)
                raise ConnectionError('normal close')
            if op == 9:
                self.frame(10, data)
                continue
            if op == 10:
                continue
            if (op == 1 and fragmented) or (op == 0 and not fragmented) or op not in (0, 1):
                raise ValueError('unsupported frame sequence')
            content.extend(data)
            if len(content) > 4096:
                raise ValueError('message limit')
            if fin:
                return json.loads(content.decode('utf-8'))
            fragmented = True
    def send_json(self, obj):
        self.state.check[obj['message_type']].validate(obj)
        self.frame(1, json.dumps(obj, separators=(',', ':')).encode())
    def do_GET(self):
        self.close_connection = True
        if self.path != '/s2':
            return self.reply(404, {})
        with self.state.lock:
            until = self.state.communications.pop(self.bearer(), 0)
        if until <= time.monotonic():
            return self.reply(401, {})
        try:
            key = base64.b64decode(self.headers.get('Sec-WebSocket-Key', ''), validate=True)
            if len(key) != 16 or self.headers.get('Sec-WebSocket-Version') != '13' or self.headers.get('Upgrade', '').lower() != 'websocket' or 'upgrade' not in self.headers.get('Connection', '').lower():
                return self.reply(400, {})
            accept = base64.b64encode(hashlib.sha1((self.headers['Sec-WebSocket-Key']+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
            self.send_response(101)
            self.send_header('Upgrade', 'websocket')
            self.send_header('Connection', 'Upgrade')
            self.send_header('Sec-WebSocket-Accept', accept)
            self.end_headers()
            self.connection.settimeout(35)
            self.exchange()
        except (ConnectionError, TimeoutError, ValueError, OSError, ValidationError) as error:
            print('Bench connection ended:', type(error).__name__, flush=True)
    def exchange(self):
        expected = {}
        def send(kind, **fields):
            identifier = str(uuid.uuid4())
            self.send_json(dict(message_type=kind, message_id=identifier, **fields))
            expected[identifier] = kind
            return identifier
        def ack(message):
            self.send_json(dict(message_type='ReceptionStatus', subject_message_id=message['message_id'], status='OK'))
        send('Handshake', role='CEM', supported_protocol_versions=[VERSION])
        rm_handshake = details = measured = selected = rejected = passed = False
        reject_id = select_id = None
        while True:
            message = self.read_message()
            if not isinstance(message, dict) or message.get('message_type') not in self.state.check:
                raise ValueError('unexpected message')
            self.state.check[message['message_type']].validate(message)
            kind = message['message_type']
            if kind == 'ReceptionStatus':
                subject = message['subject_message_id']
                if subject not in expected:
                    raise ValueError('uncorrelated acknowledgement')
                required = 'INVALID_CONTENT' if subject == reject_id else 'OK'
                if message['status'] != required:
                    raise ValueError('unexpected reception status')
                del expected[subject]
                selected |= subject == select_id
                rejected |= subject == reject_id
            elif kind == 'Handshake':
                if rm_handshake or message['role'] != 'RM' or VERSION not in message.get('supported_protocol_versions', []):
                    raise ValueError('invalid RM handshake')
                rm_handshake = True
                ack(message)
                send('HandshakeResponse', selected_protocol_version=VERSION)
            elif kind == 'ResourceManagerDetails':
                if not rm_handshake or message['available_control_types'] != ['NOT_CONTROLABLE'] or 'simulated' not in message.get('name', ''):
                    raise ValueError('not a simulated non-controllable resource')
                details = True
                ack(message)
                select_id = send('SelectControlType', control_type='NOT_CONTROLABLE')
                # Intentional negative test: no FRBC instructions or pump commands.
                reject_id = send('SelectControlType', control_type='FILL_RATE_BASED_CONTROL')
            elif kind == 'PowerMeasurement':
                if not details or message['values'] != [{'commodity_quantity': 'ELECTRIC.POWER.L1', 'value': 0}]:
                    raise ValueError('unexpected simulated measurement')
                measured = True
                ack(message)
            else:
                raise ValueError('unexpected RM message')
            print('S2 RX:', kind, flush=True)
            if rm_handshake and details and measured and selected and rejected and not expected and not passed:
                passed = True
                self.state.passed.set()
                print('S2 COMM TEST PASS: both directions, correlated responses, simulated measurement and control rejection.', flush=True)


def create_server(directory, bind):
    state = State(directory)
    server = ThreadingHTTPServer((bind, state.config['port']), Handler)
    server.daemon_threads = True
    server.state = state
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(directory/'server.pem', directory/'server-key.pem')
    server.socket = context.wrap_socket(server.socket, server_side=True)
    return server


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path(__file__).parent/'local')
    commands = parser.add_subparsers(dest='command', required=True)
    init = commands.add_parser('init')
    init.add_argument('--ip', required=True, help='PC IPv4 address reachable by the board')
    init.add_argument('--hostname', default='tulip-s2-test.invalid')
    init.add_argument('--port', type=int, default=8443)
    serve = commands.add_parser('serve')
    serve.add_argument('--bind', default='127.0.0.1', help='Use the PC LAN address to accept the board connection')
    args = parser.parse_args()
    if args.command == 'init':
        initialize(args.directory, args.ip, args.hostname, args.port)
    else:
        server = create_server(args.directory, args.bind)
        print('Bench CEM listening; credentials are not printed. Ctrl+C stops.', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            server.server_close()

if __name__ == '__main__':
    main()
