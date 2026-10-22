#!/usr/bin/env python3
"""build and run the host tests of this driver with a plain clang, no xcrun and no kernel tree

the same test list as UniXNU-sheng's check-native-adreno-memory.py, minus the native (iOS object) checks it adds on
macOS. the kits come from sibling checkouts or the environment:
  ADRENO_FIRMWARE_DIR, ARM_SMMU4K_DIR, ADRENO_PM4_DIR, SHENG_KIT_DIR   (default ../AdrenoFirmware etc. and
  ../UniXNU-sheng/package next to this repository)
  CC, CXX     compilers (default clang, clang++); CFLAGS_EXTRA/LDFLAGS_EXTRA are appended
  RUN         a runner prefix for the test binaries, e.g. qemu-aarch64 when cross compiling
  OUT         build directory (default build/host-tests under this repository)
a test's PASS line must carry the expected substring; the a650 additions sit on the same lines after the a740 text."""
import os, re, shlex, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
kit = ROOT
tests = ROOT / 'tests'
siblings = ROOT.parent
firmware = Path(os.environ.get('ADRENO_FIRMWARE_DIR', siblings / 'AdrenoFirmware'))
smmu = Path(os.environ.get('ARM_SMMU4K_DIR', siblings / 'ArmSmmu4K'))
pm4 = Path(os.environ.get('ADRENO_PM4_DIR', siblings / 'AdrenoPM4'))
sheng = Path(os.environ.get('SHENG_KIT_DIR', siblings / 'UniXNU-sheng/package'))
out = Path(os.environ.get('OUT', ROOT / 'build/host-tests'))
out.mkdir(parents=True, exist_ok=True)
cc = shlex.split(os.environ.get('CC', 'clang'))
cxx = shlex.split(os.environ.get('CXX', 'clang++'))
extra = shlex.split(os.environ.get('CFLAGS_EXTRA', ''))
ldextra = shlex.split(os.environ.get('LDFLAGS_EXTRA', ''))
run = shlex.split(os.environ.get('RUN', ''))
# the sanitizers want a runtime the cross toolchain may not have; trap mode needs none
sanitize = shlex.split(os.environ.get('SANITIZE', '-fsanitize=address,undefined'))
only = sys.argv[1:]

includes = out / 'host-include'
for name in ['IOKit/IOTypes.h', 'IOKit/IOReturn.h', 'IOKit/IOBufferMemoryDescriptor.h', 'IOKit/IOLib.h', 'kern/task.h']:
    p = includes / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text('#include "XnuMemoryFixture.h"\n')
# newer clangs warn about designated initialisers the kernel compiler accepts; the unknown-option guard keeps older ones quiet
flags = ['-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unknown-warning-option', '-Wno-missing-designated-field-initializers',
         *sanitize, *extra]
inc = ['-I', includes, '-I', tests, '-I', kit, '-I', smmu, '-I', pm4, '-I', sheng, '-I', firmware]


def build(name, sources, std='c++23'):
    binary = out / name
    cmd = [*(cxx if std.startswith('c++') else cc), '-std=' + std, *flags, *inc, *[str(s) for s in sources], *ldextra, '-o', str(binary)]
    print('build', name)
    subprocess.run([str(c) for c in cmd], check=True)
    return binary


def check(binary, expected, args=()):
    result = subprocess.run([*run, str(binary), *args], check=True, capture_output=True, text=True, timeout=600)
    print(result.stdout, end='')
    if expected not in result.stdout:
        raise SystemExit('expected coverage absent from ' + binary.name + ': ' + expected)


def objects(stems, where, std='c11'):
    done = []
    for stem in stems:
        obj = out / (stem + '-host.o')
        subprocess.run([str(c) for c in [*cc, '-std=' + std, *flags, '-c', where / (stem + '.c'), '-o', obj]], check=True)
        done.append(obj)
    return done


smmu_objects = objects(['ArmSmmu4KContext', 'ArmSmmu4KProgram', 'ArmSmmu4KLive', 'ArmSmmu4KStreams'], smmu)
mmio_object = objects(['ArmSmmu4KMmio'], smmu)
pm4_object = objects(['AdrenoPM4'], pm4)
memory = [kit / 'NativeAdrenoMemory.cpp', kit / 'NativeAdrenoSmmuMemory.cpp']
vm = memory + [kit / 'NativeAdrenoVm.cpp']
cases = [
    ('test-memory', [kit / 'NativeAdrenoMemory.cpp', tests / 'test_memory.cpp'],
     'allocation profiles'),
    ('test-smmu-memory', memory + [tests / 'test_smmu_memory.cpp', *smmu_objects],
     '34 native allocator/table-pool bindings and 4 capacity/scatter/publication/ownership refusals'),
    ('test-vm-alias', [tests / 'test_vm_alias.cpp'], '2048 oracle profiles / 524288 queries; 65536 reversed spans'),
    ('test-vm', vm + [tests / 'test_vm.cpp', *smmu_objects],
     '1 native map/hold/unmap transaction, 13 pre-ownership refusals'),
    ('test-ring', [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoRing.cpp', tests / 'test_ring.cpp', *pm4_object],
     '1 exact ring programming and submission, 1 wrap submission, 1 refused re-attach, 7 write/1 read attach failures and 4 post-commit poison cases'),
    ('test-submit', vm + [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoRing.cpp', kit / 'NativeAdrenoSubmit.cpp',
                          tests / 'test_submit.cpp', *smmu_objects, *pm4_object],
     '1 execbuf submission with exact IB packet and fence, 8 pre-reference refusals'),
    ('test-userclient', vm + [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoRing.cpp', kit / 'NativeAdrenoSubmit.cpp',
                              kit / 'NativeAdrenoUserClient.cpp', kit / 'NativeAdrenoTimer.cpp', tests / 'test_userclient.cpp',
                              *smmu_objects, *pm4_object],
     '1 complete user-client transaction with pre-submit wait'),
    ('test-timer', [kit / 'NativeAdrenoTimer.cpp', tests / 'test_timer.cpp'],
     '1 derived-offset check, 2 stable reads, 1 rollover retry, 1 I/O failure and 1 bounded torn-loop timeout'),
    ('test-gpu', [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoRing.cpp', kit / 'NativeAdrenoCpInit.cpp',
                  kit / 'NativeAdrenoIrq.cpp', kit / 'NativeAdrenoTimer.cpp', kit / 'NativeAdrenoGpu.cpp',
                  tests / 'test_gpu.cpp', *pm4_object],
     '1 full lifecycle start/stop, 3 board-step failures, 3 real component failures and 6 argument refusals'),
    ('test-client', vm + [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoRing.cpp', kit / 'NativeAdrenoSubmit.cpp',
                          kit / 'NativeAdrenoUserClient.cpp', kit / 'NativeAdrenoTimer.cpp', kit / 'NativeAdrenoClient.cpp',
                          tests / 'test_client.cpp', *smmu_objects, *pm4_object],
     '1 userspace-to-kernel round trip including CPU mapping leases and queue lifecycle'),
    ('test-board', [kit / 'NativeAdrenoBoard.cpp', tests / 'test_board.cpp'],
     '2 real SM8550 reg sets, 2 alternate cell formats, the real GPU iommu specifiers and 19 malformed/limit refusals'),
    ('test-gmumem', vm + [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoGmuMemory.cpp', tests / 'test_gmumem.cpp', *smmu_objects],
     '1 fixed a6xx GMU layout allocation and release, 5 size/range refusals'),
    ('test-gmumap', vm + [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoGmuMemory.cpp', tests / 'test_gmumap.cpp', *smmu_objects],
     '1 lower-region fixed-address GMU payload mapping and unmap, 1 pre-allocation refusal and 1 repeat-map refusal'),
    ('test-gmuconfig', vm + [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoGmuMemory.cpp', kit / 'NativeAdrenoGmuConfig.cpp',
                             tests / 'test_gmuconfig.cpp', *smmu_objects],
     '1 assembled GMU config, 7 argument refusals and 1 non-startup-profile refusal'),
    ('test-gmuplatform', [kit / 'NativeAdrenoSha256.cpp', kit / 'NativeAdrenoGmuPlatform.cpp', tests / 'test_gmuplatform.cpp'],
     '1 five-window adapter with real accesses, 11 bounds/argument refusals, 1 service binding and 1 SHA-256 vector'),
    ('test-smmuctx', memory + [kit / 'NativeAdrenoSmmuContext.cpp', tests / 'test_smmuctx.cpp', *smmu_objects],
     '1 prepared upper and 1 prepared lower SMMU context, 6 step-order refusals and 8 prepare refusals'),
    ('test-gmuctx', vm + [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoGmuMemory.cpp', kit / 'NativeAdrenoSmmuContext.cpp',
                          kit / 'NativeAdrenoGmuContext.cpp', tests / 'test_gmuctx.cpp', *smmu_objects, *mmio_object],
     '1 complete GMU context bring-up with exact routes and TTBR0, 1 non-FAULT S2CR, 1 ignored context write'),
    ('test-pwrup', [kit / 'NativeAdrenoPwrup.cpp', tests / 'test_pwrup.cpp'],
     '1 exact power-up list image, 1 A740 live-read list, 1 capacity refusal and 5 argument/count refusals'),
    ('test-irq', [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoIrq.cpp', tests / 'test_irq.cpp'],
     '1 exact A7XX mask programming, 10 decoded interrupt/status cases, 5 poison-on-unacknowledged cases'),
    ('test-cpinit', [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoRing.cpp', kit / 'NativeAdrenoCpInit.cpp',
                     tests / 'test_cpinit.cpp', *pm4_object],
     '1 exact CP_ME_INIT sequence with SQE base, 8 pre-MMIO refusals, 1 SQE write failure and 1 ring commit failure'),
    ('test-family', [kit / 'NativeAdrenoFamily.cpp', tests / 'test_family.cpp'], 'PASS: 2 families'),
    ('test-hwinit', [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoHwInit.cpp', tests / 'test_hwinit.cpp'], 'PASS: a740 and a650 hw_init programs'),
    ('test-gpuctx', vm + [kit / 'NativeAdrenoFamily.cpp', kit / 'NativeAdrenoSmmuContext.cpp', kit / 'NativeAdrenoGmuContext.cpp',
                          kit / 'NativeAdrenoGmuMemory.cpp', kit / 'NativeAdrenoGpuContext.cpp', tests / 'test_gpuctx.cpp',
                          *smmu_objects, *mmio_object],
     'PASS: a740 and a650 GPU context buffer placement'),
]
sha_test = ('test-sha256', [kit / 'NativeAdrenoSha256.cpp', kit / 'NativeAdrenoFirmware.cpp', tests / 'test_sha256.cpp'])
failures = 0
for name, sources, expected in cases:
    if only and name not in only:
        continue
    binary = build(name, sources)
    check(binary, expected)
if not only or sha_test[0] in only:
    # the firmware test wants the pinned GMU blob; without it the FIPS vectors still run through the same binary
    binary = build(*sha_test)
    blob, digest = os.environ.get('GMU_FIRMWARE'), os.environ.get('GMU_FIRMWARE_SHA256')
    if blob and digest:
        check(binary, 'validated', [blob, digest])
    else:
        print('skip', sha_test[0], 'run: set GMU_FIRMWARE=<path> GMU_FIRMWARE_SHA256=<sha256> to validate a pinned blob')
print('PASS: host tests built and run with', ' '.join(cxx), 'under', ' '.join(run) or 'the host')
