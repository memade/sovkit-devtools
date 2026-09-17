#!/usr/bin/env python3
"""Real packaged SDK: two processes, confirmed LAN pairing, messages, files, persistent reopen."""
import argparse
import hashlib
import json
from pathlib import Path
import queue
import shutil
import subprocess
import tempfile
import threading
import time

p = argparse.ArgumentParser()
p.add_argument('--console', required=True)
p.add_argument('--library', required=True)
a = p.parse_args()

class Peer:
    def __init__(self):
        self.p = subprocess.Popen([a.console, a.library], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding='utf-8')
        self.q = queue.Queue()
        def read():
            for line in self.p.stdout:
                try:
                    row = json.loads(line)
                    if 'result' in row: self.q.put(row)
                except ValueError: pass
            self.q.put(None)
        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
    def call(self, op, data=None, expected=0, **kwargs):
        self.p.stdin.write(json.dumps({'op': op, 'request': data or {}, **kwargs})+'\n'); self.p.stdin.flush()
        row = self.q.get(timeout=40)
        assert row is not None, 'console exited'
        result = row['result']
        assert result.get('code') == expected, (op, result.get('code'), result.get('message'))
        return result.get('data')
    def close(self):
        if self.p.poll() is None:
            self.p.stdin.close()
            try: assert self.p.wait(timeout=10) == 0
            except subprocess.TimeoutExpired: self.p.kill(); self.p.wait(); raise

def wait(check, timeout=15):
    end = time.monotonic()+timeout
    while time.monotonic() < end:
        value = check()
        if value: return value
        time.sleep(.08)
    raise AssertionError('condition timed out')

with tempfile.TemporaryDirectory(prefix='sovkit-devtools-test-') as directory:
    root = Path(directory) / '中文 SDK 测试'
    root.mkdir()
    library = Path(a.library).resolve()
    runtime = root / 'runtime'
    runtime.mkdir()
    for item in library.parent.iterdir():
        if item.is_file() and (item.suffix.lower() in ('.dll', '.dylib') or '.so' in item.name):
            shutil.copy2(item, runtime / item.name)
    a.library = str(runtime / library.name)
    left, right = Peer(), Peer()
    try:
        identity = left.call('start', profile=str(root/'left'), password='isolated-test-only', deviceName='Left')['identity']
        right.call('start', profile=str(root/'left'), password='isolated-test-only', expected=-1)
        right.call('start', deviceName='Right')
        left.call('discovery_start', {'interfaceAddress': '127.0.0.1'})
        state = right.call('discovery_start', {'interfaceAddress': '127.0.0.1'})
        port = state['pairingPort']
        left.call('pairing_start', {'address': '127.0.0.1', 'pairingPort': port})
        wait(lambda: left.call('pairing_status').get('state') == 'awaiting_confirmation' and right.call('pairing_status').get('state') == 'awaiting_confirmation')
        assert left.call('pairing_status')['safetyCode'] == right.call('pairing_status')['safetyCode']
        left.call('pairing_confirm', {'accept': True}); right.call('pairing_confirm', {'accept': True})
        left_rel = wait(lambda: left.call('relationship_list'))[0]['relationshipId']
        right_rel = wait(lambda: right.call('relationship_list'))[0]['relationshipId']
        sent = left.call('message_send', {'relationshipId': left_rel, 'text': 'devtools 中文消息🙂'})
        def received():
            right.call('message_flush')
            rows = right.call('message_list', {'relationshipId': right_rel})
            return any(x.get('text') == 'devtools 中文消息🙂' for x in rows)
        wait(received)
        def delivered():
            right.call('message_flush'); left.call('message_flush')
            return any(x['messageId'] == sent['messageId'] and x['state'] == 'delivered'
                       for x in left.call('message_list', {'relationshipId': left_rel}))
        wait(delivered)
        source = root/'中文 payload.bin'; source.write_bytes(bytes(range(256))*80)
        inbox = root/'inbox'; inbox.mkdir()
        left.call('transfer_offer', {'relationshipId': left_rel, 'sourcePaths': [str(source)], 'logicalNames': ['中文 payload.bin']})
        def incoming():
            right.call('transfer_flush')
            return right.call('transfer_list')
        transfer = wait(incoming)[0]
        right.call('transfer_decide', {'transferId': transfer['transferId'], 'accept': True, 'destinationDirectory': str(inbox)})
        def copied():
            left.call('transfer_flush'); right.call('transfer_flush')
            return (inbox/'中文 payload.bin').is_file() and (inbox/'中文 payload.bin').read_bytes() == source.read_bytes()
        wait(copied, 25)
        def completed():
            left.call('transfer_flush'); right.call('transfer_flush')
            states = [p.call('transfer_list')[0]['state'] for p in (left, right)]
            return all(state == 'completed' for state in states)
        wait(completed)
        left.call('stop'); right.call('stop')
        left.close(); left = Peer()
        before = {name: hashlib.sha256((root/'left'/name).read_bytes()).digest()
                  for name in ['vault.enc', 'keystore.enc', 'sovkit.db']}
        left.call('start', profile=str(root/'left'), password='wrong-test-password', expected=-1)
        assert before == {name: hashlib.sha256((root/'left'/name).read_bytes()).digest() for name in before}
        reopened = left.call('start', profile=str(root/'left'), password='isolated-test-only', deviceName='Left')['identity']
        assert reopened == identity
        assert left.call('relationship_list')[0]['relationshipId'] == left_rel
        left.call('stop')
        print('PASS: real LAN pairing, SAS, message, exact file bytes, encrypted reopen, directory lock, wrong-password data retention')
    finally:
        left.close(); right.close()
