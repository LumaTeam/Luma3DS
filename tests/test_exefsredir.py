"""Execute the built ARM installer and payload, with FS IPC supplied by a stub.

Usage: python3 tests/test_exefsredir.py sysmodules/loader/loader.elf
Requires unicorn and pyelftools. No Nintendo binaries or content are used.
This verifies machine code and the IPC contract, not HOME Menu compatibility.
"""
import struct
import sys
import unittest

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_INTR
from unicorn.arm_const import (
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
    UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
    UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11,
    UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_C13_C0_3,
)

ELF_PATH = sys.argv.pop(1)
REGS = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
        UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
        UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11,
        UC_ARM_REG_R12]
CODE, DATA, STACK, STOP = 0x20000000, 0x30000000, 0x31000000, 0x32000000
TLS, ARCHIVE, FILE = DATA, DATA + 0x1000, DATA + 0x2000


def pack(words):
    return struct.pack('<' + 'I' * len(words), *words)


def branch(at, target):
    return 0xEB000000 | (((target - at - 8) // 4) & 0xFFFFFF)


class ExefsTests(unittest.TestCase):
    def setUp(self):
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        with open(ELF_PATH, 'rb') as stream:
            elf = ELFFile(stream)
            self.symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
            pages = set()
            for seg in elf.iter_segments():
                if seg['p_type'] != 'PT_LOAD':
                    continue
                start, end = seg['p_vaddr'], seg['p_vaddr'] + seg['p_memsz']
                for page in range(start & ~4095, (end + 4095) & ~4095, 4096):
                    if page not in pages:
                        self.uc.mem_map(page, 4096)
                        pages.add(page)
                self.uc.mem_write(start, seg.data())
        for addr, size in [(CODE, 0x4000), (DATA, 0x4000), (STACK, 0x10000), (STOP, 0x1000)]:
            self.uc.mem_map(addr, size)
        self.uc.reg_write(UC_ARM_REG_C13_C0_3, TLS)
        self.uc.reg_write(UC_ARM_REG_SP, STACK + 0xF000)
        self.uc.reg_write(UC_ARM_REG_LR, STOP)
        self.calls = []
        self.responses = [(0, 0)]
        self.uc.hook_add(UC_HOOK_INTR, self.service)

    def service(self, uc, interrupt, _):
        self.assertEqual(interrupt, 2)  # ARM SVC
        cmd = list(struct.unpack('<13I', uc.mem_read(TLS + 0x80, 52)))
        archive = bytes(uc.mem_read(cmd[10], cmd[4]))
        path = bytes(uc.mem_read(cmd[12], cmd[6]))
        self.calls.append((cmd, archive, path, uc.reg_read(UC_ARM_REG_R0)))
        transport, result = self.responses[min(len(self.calls) - 1, len(self.responses) - 1)]
        # Overwrite every request word to exercise complete fallback restoration.
        uc.mem_write(TLS + 0x80, pack([0x8030042, result, 0, 0x1234] + [0xBAD] * 9))
        uc.reg_write(UC_ARM_REG_R0, transport)
        # SVC caller-saved registers must not be used to recover the original call.
        for reg in REGS[1:4]:
            uc.reg_write(reg, 0xBADBAD)

    def fixture(self):
        data = bytearray(0x1000)
        for offset, value in {
            0x20: 0xE92D4010, 0x24: 0xE59F1014, 0x28: branch(0x28, 0x80),
            0x2C: 0xE8BD8010, 0x40: 0x08030204, 0x80: 0xEF000032, 0x84: 0xE12FFF1E,
        }.items():
            struct.pack_into('<I', data, offset, value)
        return data

    def install(self, data=None, size=0x1000, text=0x200, reserve=512):
        if data is None:
            data = self.fixture()
        self.uc.mem_write(CODE, bytes(data))
        for reg, val in zip(REGS, [CODE, size, text, reserve]):
            self.uc.reg_write(reg, val)
        self.uc.reg_write(UC_ARM_REG_LR, STOP)
        self.uc.emu_start(self.symbols['patchHomeMenuExefs'], STOP, count=100000)
        return self.uc.reg_read(UC_ARM_REG_R0), bytes(self.uc.mem_read(CODE, len(data)))

    def prepare(self, name='icon', title=0x0004000000086300, media=1):
        archive = pack([title & 0xFFFFFFFF, title >> 32, media, 0])
        file = pack([0, 0, 2]) + name.encode().ljust(8, b'\0')
        self.uc.mem_write(ARCHIVE, archive)
        self.uc.mem_write(FILE, file)
        cmd = [0x08030204, 0, 0x2345678A, 2, 16, 2, 20, 1, 0,
               0x40802, ARCHIVE, 0x50002, FILE]
        self.uc.mem_write(TLS + 0x80, pack(cmd))
        return cmd, archive, file

    def run_payload(self):
        initial = [0x71] + [0xAB00 + n for n in range(1, 13)]
        for reg, value in zip(REGS, initial):
            self.uc.reg_write(reg, value)
        self.uc.reg_write(UC_ARM_REG_LR, STOP)
        sp = self.uc.reg_read(UC_ARM_REG_SP)
        self.uc.mem_write(sp - 176, b'G' * 16)
        self.uc.mem_write(sp, b'G' * 16)
        payload = self.symbols['exefsRedirPatch']
        length = self.symbols['exefsRedirPatchEnd'] - payload
        # Execute a relocated copy, as HOME Menu does, to catch absolute references.
        self.uc.mem_write(CODE + 0x2000, bytes(self.uc.mem_read(payload, length)))
        self.uc.emu_start(CODE + 0x2000, STOP, count=10000)
        self.assertEqual(self.uc.reg_read(UC_ARM_REG_SP), sp)
        self.assertEqual(bytes(self.uc.mem_read(sp - 176, 16)), b'G' * 16)
        self.assertEqual(bytes(self.uc.mem_read(sp, 16)), b'G' * 16)
        self.assertEqual(self.uc.reg_read(UC_ARM_REG_LR), STOP)
        for reg, value in zip(REGS[4:], initial[4:]):
            self.assertEqual(self.uc.reg_read(reg), value)
        self.assertTrue(all(call[3] == initial[0] for call in self.calls))

    def test_installer(self):
        ok, data = self.install()
        self.assertEqual(ok, 1)
        insn = struct.unpack_from('<I', data, 0x28)[0]
        target = 0x28 + 8 + ((insn & 0xFFFFFF) << 2)
        payload = self.symbols['exefsRedirPatch']
        length = self.symbols['exefsRedirPatchEnd'] - payload
        self.assertEqual(data[target:target + length], bytes(self.uc.mem_read(payload, length)))
        expected = self.fixture()
        expected[0x28:0x2C] = data[0x28:0x2C]
        expected[target:target + length] = data[target:target + length]
        self.assertEqual(data, expected)

    def test_installer_rejects_without_writes(self):
        variants = []
        for offset, value in [(0x24, 0xE1A00000), (0x24, 0xE59F1FFF),
                              (0x24, 0xE59F1015), (0x20, 0xE92D0010),
                              (0x84, 0xE1A00000), (0xFFC, 1)]:
            data = self.fixture()
            struct.pack_into('<I', data, offset, value)
            variants.append((data, {}))
        ambiguous = self.fixture()
        struct.pack_into('<I', ambiguous, 0x30, branch(0x30, 0x80))
        variants.append((ambiguous, {}))
        for kwargs in [dict(size=0x100), dict(text=0), dict(text=0x201), dict(text=0x1000),
                       dict(reserve=0x1000), dict(text=0xFFFFFFFC, size=0xFFFFFFFF)]:
            variants.append((self.fixture(), kwargs))
        for data, kwargs in variants:
            with self.subTest(kwargs=kwargs, changed=[i for i, v in enumerate(data) if v]):
                ok, after = self.install(data, **kwargs)
                self.assertEqual(ok, 0)
                self.assertEqual(after, data)

    def test_installer_preserves_romfs_padding(self):
        data = self.fixture()
        data[0x200:0x400] = b'R' * 512
        ok, after = self.install(data)
        self.assertEqual(ok, 1)
        self.assertEqual(after[0x200:0x400], b'R' * 512)
        ok, again = self.install(after)
        self.assertEqual(ok, 0)
        self.assertEqual(again, after)

    def test_installer_backward_svc_branch(self):
        data = self.fixture()
        struct.pack_into('<II', data, 0, 0xEF000032, 0xE12FFF1E)
        struct.pack_into('<I', data, 0x28, branch(0x28, 0))
        ok, _ = self.install(data)
        self.assertEqual(ok, 1)

    def test_installer_ambiguous_functions(self):
        data = self.fixture()
        for at, value in {0x100: 0xE92D4010, 0x104: 0xE59F1014,
                          0x108: branch(0x108, 0x80), 0x120: 0x08030204}.items():
            struct.pack_into('<I', data, at, value)
        ok, after = self.install(data)
        self.assertEqual(ok, 0)
        self.assertEqual(after, data)

    def test_success_paths(self):
        for name in ['icon', 'banner']:
            for media in [0, 1, 2]:
                for title in [0x0004000000086300, 0x00040002ABCDEF00]:
                    with self.subTest(name=name, media=media, title=title):
                        self.calls.clear()
                        self.prepare(name, title, media)
                        self.run_payload()
                        self.assertEqual(len(self.calls), 1)
                        cmd, archive, path, _ = self.calls[0]
                        self.assertEqual(path, f'/luma/titles/{title:016X}/exefs/{name}\0'.encode())
                        self.assertEqual(archive, b'\0')
                        self.assertEqual(cmd[:9], [0x08030204, 0, 9, 1, 1, 3, len(path), 1, 0])
                        self.assertEqual(cmd[9], 0x4802)
                        self.assertEqual(cmd[11], (len(path) << 14) | 2)
                        self.assertEqual(self.uc.reg_read(UC_ARM_REG_R0), 0)

    def test_fallback(self):
        for transport, result in [(0, 0xC8804478), (0xD9001830, 0), (0, 0xD900458B)]:
            with self.subTest(transport=transport, result=result):
                self.calls.clear()
                original = self.prepare('banner')
                self.responses = [(transport, result), (0xD9009999, 0xD9008888)]
                self.run_payload()
                self.assertEqual(len(self.calls), 2)
                self.assertEqual(self.calls[1][:3], original)
                self.assertEqual(self.uc.reg_read(UC_ARM_REG_R0), 0xD9009999)

    def test_unrelated_requests(self):
        changes = [(0, 0x80201C2), (1, 1), (2, 3), (3, 3), (4, 12), (5, 3),
                   (6, 12), (7, 3), (8, 1), (9, 0), (11, 0)]
        for word, value in changes:
            with self.subTest(word=word):
                self.calls.clear()
                cmd, archive, file = self.prepare()
                cmd[word] = value
                self.uc.mem_write(TLS + 0x80, pack(cmd))
                self.run_payload()
                self.assertEqual(len(self.calls), 1)
                self.assertEqual(self.calls[0][0], cmd)
        for name in ['logo', '.code', 'iconx', 'bannerx']:
            with self.subTest(name=name):
                self.calls.clear()
                original = self.prepare(name)
                self.run_payload()
                self.assertEqual(len(self.calls), 1)
                self.assertEqual(self.calls[0][:3], original)
        for ptr, word, value in [(ARCHIVE, 1, 0x00040030), (ARCHIVE, 1, 0x0004000E),
                                  (ARCHIVE, 2, 3), (ARCHIVE, 3, 1),
                                  (FILE, 0, 1), (FILE, 1, 1), (FILE, 2, 0)]:
            with self.subTest(ptr=ptr, word=word, value=value):
                self.calls.clear()
                cmd, _, _ = self.prepare()
                self.uc.mem_write(ptr + word * 4, pack([value]))
                self.run_payload()
                self.assertEqual(len(self.calls), 1)
                self.assertEqual(self.calls[0][0], cmd)


if __name__ == '__main__':
    unittest.main(verbosity=2)
