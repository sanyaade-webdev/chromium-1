#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The deep heap profiler script for Chrome."""

from collections import defaultdict
import os
import re
import subprocess
import sys
import tempfile

BUCKET_ID = 5
VIRTUAL = 0
COMMITTED = 1
ALLOC_COUNT = 2
FREE_COUNT = 3
NULL_REGEX = re.compile('')
PPROF_PATH = os.path.join(os.path.dirname(__file__),
                          os.pardir,
                          os.pardir,
                          'third_party',
                          'tcmalloc',
                          'chromium',
                          'src',
                          'pprof')

# Heap Profile Dump versions

# DUMP_DEEP_1 DOES NOT distinct mmap regions and malloc chunks.
# Their stacktraces DO contain mmap* or tc-* at their tops.
# They should be processed by POLICY_DEEP_1.
DUMP_DEEP_1 = 'DUMP_DEEP_1'

# DUMP_DEEP_2 DOES distinct mmap regions and malloc chunks.
# Their stacktraces still DO contain mmap* or tc-*.
# They should be processed by POLICY_DEEP_1.
DUMP_DEEP_2 = 'DUMP_DEEP_2'

# DUMP_DEEP_3 DOES distinct mmap regions and malloc chunks.
# Their stacktraces DO NOT contain mmap* or tc-*.
# They should be processed by POLICY_DEEP_2.
DUMP_DEEP_3 = 'DUMP_DEEP_3'

# Heap Profile Policy versions

# POLICY_DEEP_1 DOES NOT include allocation_type columns.
# mmap regions are distincted w/ mmap frames in the pattern column.
POLICY_DEEP_1 = 'POLICY_DEEP_1'

# POLICY_DEEP_2 DOES include allocation_type columns.
# mmap regions are distincted w/ the allocation_type column.
POLICY_DEEP_2 = 'POLICY_DEEP_2'

# TODO(dmikurube): Avoid global variables.
address_symbol_dict = {}
components = []


class Policy(object):

  def __init__(self, name, mmap, pattern):
    self.name = name
    self.mmap = mmap
    self.condition = re.compile(pattern + r'\Z')


def get_component(policy_list, bucket, mmap):
  """Returns a component name which a given bucket belongs to.

  Args:
      policy_list: A list containing Policy objects.  (Parsed policy data by
          parse_policy.)
      bucket: A Bucket object to be searched for.
      mmap: True if searching for a mmap region.

  Returns:
      A string representing a component name.
  """
  if not bucket:
    return 'no-bucket'
  if bucket.component:
    return bucket.component

  stacktrace = ''.join(
      address_symbol_dict[a] + ' ' for a in bucket.stacktrace).strip()

  for policy in policy_list:
    if mmap == policy.mmap and policy.condition.match(stacktrace):
      bucket.component = policy.name
      return policy.name

  assert False


class Bucket(object):

  def __init__(self, stacktrace):
    self.stacktrace = stacktrace
    self.component = ''


class Log(object):

  """A class representing one dumped log data."""
  def __init__(self, log_path, buckets):
    self.log_path = log_path
    with open(self.log_path, mode='r') as log_f:
      self.log_lines = log_f.readlines()
    self.log_version = ''
    sys.stderr.write('parsing a log file:%s\n' % log_path)
    self.mmap_stacktrace_lines = []
    self.malloc_stacktrace_lines = []
    self.counters = {}
    self.log_time = os.stat(self.log_path).st_mtime
    self.parse_log(buckets)

  @staticmethod
  def dump_stacktrace_lines(stacktrace_lines, buckets):
    """Prints a given stacktrace.

    Args:
        stacktrace_lines: A list of strings which are valid as stacktraces.
        buckets: A dict mapping bucket ids and their corresponding Bucket
            objects.
    """
    for l in stacktrace_lines:
      words = l.split()
      bucket = buckets[int(words[BUCKET_ID])]
      if not bucket:
        continue
      for i in range(0, BUCKET_ID - 1):
        sys.stdout.write(words[i] + ' ')
      for address in bucket.stacktrace:
        sys.stdout.write((address_symbol_dict.get(address) or address) + ' ')
      sys.stdout.write('\n')

  def dump_stacktrace(self, buckets):
    """Prints stacktraces contained in the log.

    Args:
        buckets: A dict mapping bucket ids and their corresponding Bucket
            objects.
    """
    self.dump_stacktrace_lines(self.mmap_stacktrace_lines, buckets)
    self.dump_stacktrace_lines(self.malloc_stacktrace_lines, buckets)

  @staticmethod
  def accumulate_size_for_pprof(stacktrace_lines, policy_list, buckets,
                                component_name, mmap):
    """Accumulates size of committed chunks and the number of allocated chunks.

    Args:
        stacktrace_lines: A list of strings which are valid as stacktraces.
        policy_list: A list containing Policy objects.  (Parsed policy data by
            parse_policy.)
        buckets: A dict mapping bucket ids and their corresponding Bucket
            objects.
        component_name: A name of component for filtering.
        mmap: True if searching for a mmap region.

    Returns:
        Two integers which are the accumulated size of committed regions and the
        number of allocated chunks, respectively.
    """
    com_committed = 0
    com_allocs = 0
    for l in stacktrace_lines:
      words = l.split()
      bucket = buckets[int(words[BUCKET_ID])]
      if (not bucket or
          (component_name and
           component_name != get_component(policy_list, bucket, mmap))):
        continue

      com_committed += int(words[COMMITTED])
      com_allocs += int(words[ALLOC_COUNT]) - int(words[FREE_COUNT])

    return com_committed, com_allocs

  @staticmethod
  def dump_stacktrace_lines_for_pprof(stacktrace_lines, policy_list,
                                      buckets, component_name, mmap):
    """Prints information of stacktrace lines for pprof.

    Args:
        stacktrace_lines: A list of strings which are valid as stacktraces.
        policy_list: A list containing Policy objects.  (Parsed policy data by
            parse_policy.)
        buckets: A dict mapping bucket ids and their corresponding Bucket
            objects.
        component_name: A name of component for filtering.
        mmap: True if searching for a mmap region.
    """
    for l in stacktrace_lines:
      words = l.split()
      bucket = buckets[int(words[BUCKET_ID])]
      if (not bucket or
          (component_name and
           component_name != get_component(policy_list, bucket, mmap))):
        continue

      sys.stdout.write('%6d: %8s [%6d: %8s] @' % (
          int(words[ALLOC_COUNT]) - int(words[FREE_COUNT]),
          words[COMMITTED],
          int(words[ALLOC_COUNT]) - int(words[FREE_COUNT]),
          words[COMMITTED]))
      for address in bucket.stacktrace:
        sys.stdout.write(' ' + address)
      sys.stdout.write('\n')

  def dump_for_pprof(self, policy_list, buckets, mapping_lines, component_name):
    """Converts the log file so it can be processed by pprof.

    Args:
        policy_list: A list containing Policy objects.  (Parsed policy data by
            parse_policy.)
        buckets: A dict mapping bucket ids and their corresponding Bucket
            objects.
        mapping_lines: A list of strings containing /proc/.../maps.
        component_name: A name of component for filtering.
    """
    sys.stdout.write('heap profile: ')
    com_committed, com_allocs = self.accumulate_size_for_pprof(
        self.mmap_stacktrace_lines, policy_list, buckets, component_name,
        True)
    add_committed, add_allocs = self.accumulate_size_for_pprof(
        self.malloc_stacktrace_lines, policy_list, buckets, component_name,
        False)
    com_committed += add_committed
    com_allocs += add_allocs

    sys.stdout.write('%6d: %8s [%6d: %8s] @ heapprofile\n' % (
        com_allocs, com_committed, com_allocs, com_committed))

    self.dump_stacktrace_lines_for_pprof(
        self.mmap_stacktrace_lines, policy_list, buckets, component_name,
        True)
    self.dump_stacktrace_lines_for_pprof(
        self.malloc_stacktrace_lines, policy_list, buckets, component_name,
        False)

    sys.stdout.write('MAPPED_LIBRARIES:\n')
    for l in mapping_lines:
      sys.stdout.write(l)

  @staticmethod
  def check_stacktrace_line(stacktrace_line, buckets):
    """Checks if a given stacktrace_line is valid as stacktrace.

    Args:
        stacktrace_line: A string to be checked.
        buckets: A dict mapping bucket ids and their corresponding Bucket
            objects.

    Returns:
        True if the given stacktrace_line is valid.
    """
    words = stacktrace_line.split()
    if len(words) < BUCKET_ID + 1:
      return False
    if words[BUCKET_ID - 1] != '@':
      return False
    bucket = buckets[int(words[BUCKET_ID])]
    if bucket:
      for address in bucket.stacktrace:
        address_symbol_dict[address] = ''
    return True

  @staticmethod
  def skip_lines_while(line_number, max_line_number, skipping_condition):
    """Increments line_number until skipping_condition(line_number) is false.
    """
    while skipping_condition(line_number):
      line_number += 1
      if line_number >= max_line_number:
        sys.stderr.write('invalid heap profile dump.')
        return line_number
    return line_number

  def parse_stacktraces_while_valid(self, buckets, log_lines, ln):
    """Parses stacktrace lines while the lines are valid.

    Args:
        buckets: A dict mapping bucket ids and their corresponding Bucket
            objects.
        log_lines: A list of lines to be parsed.
        ln: An integer representing the starting line number in log_lines.

    Returns:
        A pair of a list of valid lines and an integer representing the last
        line number in log_lines.
    """
    ln = self.skip_lines_while(
        ln, len(log_lines), lambda n: not log_lines[n].split()[0].isdigit())
    stacktrace_lines_start = ln
    ln = self.skip_lines_while(
        ln, len(log_lines),
        lambda n: self.check_stacktrace_line(log_lines[n], buckets))
    return (log_lines[stacktrace_lines_start:ln], ln)

  def parse_stacktraces(self, buckets):
    """Parses lines in self.log_lines as stacktrace.

    Valid stacktrace lines are stored into self.mmap_stacktrace_lines and
    self.malloc_stacktrace_lines.

    Args:
        buckets: A dict mapping bucket ids and their corresponding Bucket
            objects.

    Returns:
        A string representing a version of the stacktrace dump.  '' for invalid
        dump.
    """
    version = ''

    # Skip until an identifiable line.
    headers = ('STACKTRACES:\n', 'MMAP_STACKTRACES:\n', 'heap profile: ')
    ln = self.skip_lines_while(
        0, len(self.log_lines),
        lambda n: not self.log_lines[n].startswith(headers))

    # Identify a version.
    if self.log_lines[ln].startswith('heap profile: '):
      version = self.log_lines[ln][13:].strip()
      if version == DUMP_DEEP_2 or version == DUMP_DEEP_3:
        ln = self.skip_lines_while(
            ln, len(self.log_lines),
            lambda n: self.log_lines[n] != 'MMAP_STACKTRACES:\n')
      else:
        sys.stderr.write('  invalid heap profile dump version:%s\n' % version)
        return ''
    elif self.log_lines[ln] == 'STACKTRACES:\n':
      version = DUMP_DEEP_1
    elif self.log_lines[ln] == 'MMAP_STACKTRACES:\n':
      version = DUMP_DEEP_2

    if version == DUMP_DEEP_3:
      sys.stderr.write('  heap profile dump version: %s\n' % version)
      (self.mmap_stacktrace_lines, ln) = self.parse_stacktraces_while_valid(
          buckets, self.log_lines, ln)
      ln = self.skip_lines_while(
          ln, len(self.log_lines),
          lambda n: self.log_lines[n] != 'MALLOC_STACKTRACES:\n')
      (self.malloc_stacktrace_lines, ln) = self.parse_stacktraces_while_valid(
          buckets, self.log_lines, ln)
      return version

    elif version == DUMP_DEEP_2:
      sys.stderr.write('  heap profile dump version: %s\n' % version)
      (self.mmap_stacktrace_lines, ln) = self.parse_stacktraces_while_valid(
          buckets, self.log_lines, ln)
      ln = self.skip_lines_while(
          ln, len(self.log_lines),
          lambda n: self.log_lines[n] != 'MALLOC_STACKTRACES:\n')
      (self.malloc_stacktrace_lines, ln) = self.parse_stacktraces_while_valid(
          buckets, self.log_lines, ln)
      self.malloc_stacktrace_lines.extend(self.mmap_stacktrace_lines)
      self.mmap_stacktrace_lines = []
      return version

    elif version == DUMP_DEEP_1:
      sys.stderr.write('  heap profile dump version: %s\n' % version)
      (self.malloc_stacktrace_lines, ln) = self.parse_stacktraces_while_valid(
          buckets, self.log_lines, ln)
      return version

    else:
      sys.stderr.write('  invalid heap profile dump version:%s\n' % version)
      return ''

  def parse_global_stats(self):
    """Parses lines in self.log_lines as global stats."""
    ln = self.skip_lines_while(
        0, len(self.log_lines),
        lambda n: self.log_lines[n] != 'GLOBAL_STATS:\n')

    for prefix in ['total', 'file', 'anonymous', 'other', 'mmap', 'tcmalloc']:
      ln = self.skip_lines_while(
          ln, len(self.log_lines),
          lambda n: self.log_lines[n].split()[0] != prefix)
      words = self.log_lines[ln].split()
      self.counters[prefix + '_virtual'] = int(words[-2])
      self.counters[prefix + '_committed'] = int(words[-1])

  def parse_log(self, buckets):
    self.parse_global_stats()
    self.log_version = self.parse_stacktraces(buckets)

  @staticmethod
  def accumulate_size_for_policy(stacktrace_lines,
                                 policy_list, buckets, sizes, mmap):
    for l in stacktrace_lines:
      words = l.split()
      bucket = buckets[int(words[BUCKET_ID])]
      component_match = get_component(policy_list, bucket, mmap)
      sizes[component_match] += int(words[COMMITTED])

      if component_match.startswith('tc-'):
        sizes['tc-total-log'] += int(words[COMMITTED])
      elif component_match.startswith('mmap-'):
        sizes['mmap-total-log'] += int(words[COMMITTED])
      else:
        sizes['other-total-log'] += int(words[COMMITTED])

  def apply_policy(self, policy_list, buckets, first_log_time):
    """Aggregates the total memory size of each component.

    Iterate through all stacktraces and attribute them to one of the components
    based on the policy.  It is important to apply policy in right order.

    Args:
        policy_list: A list containing Policy objects.  (Parsed policy data by
            parse_policy.)
        buckets: A dict mapping bucket ids and their corresponding Bucket
            objects.
        first_log_time: An integer representing time when the first log is
            dumped.

    Returns:
        A dict mapping components and their corresponding sizes.
    """

    sys.stderr.write('apply policy:%s\n' % (self.log_path))
    sizes = dict((c, 0) for c in components)

    self.accumulate_size_for_policy(self.mmap_stacktrace_lines,
                                    policy_list, buckets, sizes, True)
    self.accumulate_size_for_policy(self.malloc_stacktrace_lines,
                                    policy_list, buckets, sizes, False)

    sizes['mmap-no-log'] = self.counters['mmap_committed'] - sizes[
        'mmap-total-log']
    sizes['mmap-total-record'] = self.counters['mmap_committed']
    sizes['mmap-total-record-vm'] = self.counters['mmap_virtual']

    sizes['tc-no-log'] = self.counters['tcmalloc_committed'] - sizes[
        'tc-total-log']
    sizes['tc-total-record'] = self.counters['tcmalloc_committed']
    sizes['tc-unused'] = sizes['mmap-tcmalloc'] - self.counters[
        'tcmalloc_committed']
    sizes['tc-total'] = sizes['mmap-tcmalloc']

    for key, value in { 'total': 'total_committed',
                        'filemapped': 'file_committed',
                        'anonymous': 'anonymous_committed',
                        'other': 'other_committed',
                        'total-vm': 'total_virtual',
                        'filemapped-vm': 'file_virtual',
                        'anonymous-vm': 'anonymous_virtual',
                        'other-vm': 'other_virtual' }.items():
      if key in sizes:
        sizes[key] = self.counters[value]

    if 'unknown' in sizes:
      sizes['unknown'] = self.counters['total_committed'] - self.counters[
          'mmap_committed']
    if 'total-exclude-profiler' in sizes:
      sizes['total-exclude-profiler'] = self.counters[
          'total_committed'] - sizes['mmap-profiler']
    if 'hour' in sizes:
      sizes['hour'] = (self.log_time - first_log_time) / 60.0 / 60.0
    if 'minute' in sizes:
      sizes['minute'] = (self.log_time - first_log_time) / 60.0
    if 'second' in sizes:
      sizes['second'] = self.log_time - first_log_time

    return sizes

  @staticmethod
  def accumulate_size_for_expand(stacktrace_lines, policy_list, buckets,
                                 component_name, depth, sizes, mmap):
    for line in stacktrace_lines:
      words = line.split()
      bucket = buckets[int(words[BUCKET_ID])]
      component_match = get_component(policy_list, bucket, mmap)
      if component_match == component_name:
        stacktrace_sequence = ''
        for address in bucket.stacktrace[1 : min(len(bucket.stacktrace),
                                                 1 + depth)]:
          stacktrace_sequence += address_symbol_dict[address] + ' '
        if not stacktrace_sequence in sizes:
          sizes[stacktrace_sequence] = 0
        sizes[stacktrace_sequence] += int(words[COMMITTED])

  def expand(self, policy_list, buckets, component_name, depth):
    """Prints all stacktraces in a given component of given depth.

    Args:
        policy_list: A list containing Policy objects.  (Parsed policy data by
            parse_policy.)
        buckets: A dict mapping bucket ids and their corresponding Bucket
            objects.
        component_name: A name of component for filtering.
        depth: An integer representing depth to be printed.
    """
    sizes = {}

    self.accumulate_size_for_expand(
        self.mmap_stacktrace_lines, policy_list, buckets, component_name,
        depth, sizes, True)
    self.accumulate_size_for_expand(
        self.malloc_stacktrace_lines, policy_list, buckets, component_name,
        depth, sizes, False)

    sorted_sizes_list = sorted(
        sizes.iteritems(), key=(lambda x: x[1]), reverse=True)
    total = 0
    for size_pair in sorted_sizes_list:
      sys.stdout.write('%10d %s\n' % (size_pair[1], size_pair[0]))
      total += size_pair[1]
    sys.stderr.write('total: %d\n' % (total))


def read_symbols(symbol_path, mapping_lines, chrome_path):
  """Reads symbol names from a .symbol file or a Chrome binary with pprof.

  Args:
      symbol_path: A string representing a path for a .symbol file.
      mapping_lines: A list of strings containing /proc/.../maps.
      chrome_path: A string representing a path for a Chrome binary.
  """
  with open(symbol_path, mode='a+') as symbol_f:
    symbol_lines = symbol_f.readlines()

    if not symbol_lines:
      with tempfile.NamedTemporaryFile(
          suffix='maps', prefix="dmprof", mode='w+') as pprof_in:
        with tempfile.NamedTemporaryFile(
            suffix='symbols', prefix="dmprof", mode='w+') as pprof_out:
          for line in mapping_lines:
            pprof_in.write(line)

          address_list = sorted(address_symbol_dict)
          for key in address_list:
            pprof_in.write(key + '\n')

          pprof_in.seek(0)

          p = subprocess.Popen(
              '%s --symbols %s' % (PPROF_PATH, chrome_path),
              shell=True, stdin=pprof_in, stdout=pprof_out)
          p.wait()

          pprof_out.seek(0)
          symbols = pprof_out.readlines()
          for address, symbol in zip(address_list, symbols):
            address_symbol_dict[address] = symbol.strip()

      for address, symbol in address_symbol_dict.iteritems():
        symbol_f.write('%s %s\n' % (address, symbol))
    else:
      for l in symbol_lines:
        items = l.split()
        address_symbol_dict[items[0]] = items[1]


def parse_policy(policy_path):
  """Parses policy file.

  A policy file contains component's names and their
  stacktrace pattern written in regular expression.
  Those patterns are matched against each symbols of
  each stacktraces in the order written in the policy file

  Args:
       policy_path: A path for a policy file.
  Returns:
       A list containing component's name and its regex object
  """
  with open(policy_path, mode='r') as policy_f:
    policy_lines = policy_f.readlines()

  policy_version = POLICY_DEEP_1
  if policy_lines[0].startswith('heap profile policy: '):
    policy_version = policy_lines[0][21:].strip()
    policy_lines.pop(0)
  policy_list = []

  if policy_version == POLICY_DEEP_2 or policy_version == POLICY_DEEP_1:
    sys.stderr.write('  heap profile policy version: %s\n' % policy_version)
    for line in policy_lines:
      if line[0] == '#':
        continue

      if policy_version == POLICY_DEEP_2:
        (name, allocation_type, pattern) = line.strip().split(None, 2)
        mmap = False
        if allocation_type == 'mmap':
          mmap = True
      elif policy_version == POLICY_DEEP_1:
        name = line.split()[0]
        pattern = line[len(name) : len(line)].strip()
        mmap = False

      if pattern != 'default':
        policy_list.append(Policy(name, mmap, pattern))
      if components.count(name) == 0:
        components.append(name)

  else:
    sys.stderr.write('  invalid heap profile policy version: %s\n' % (
        policy_version))

  return policy_list


def main():
  if (len(sys.argv) < 4) or (not (sys.argv[1] in ['--csv',
                                                  '--expand',
                                                  '--list',
                                                  '--stacktrace',
                                                  '--pprof'])):
    sys.stderr.write("""Usage:
%s [options] <chrome-binary> <policy> <profile> [component-name] [depth]

Options:
  --csv                         Output result in csv format
  --stacktrace                  Convert raw address to symbol names
  --list                        Lists components and their sizes
  --expand                      Show all stacktraces in the specified component
                                of given depth with their sizes
  --pprof                       Format the profile file so it can be processed
                                by pprof

Examples:
  dmprof --csv Debug/chrome dmpolicy hprof.12345.0001.heap > result.csv
  dmprof --list Debug/chrome dmpolicy hprof.12345.0012.heap
  dmprof --expand Debug/chrome dmpolicy hprof.12345.0012.heap tc-webkit 4
  dmprof --pprof Debug/chrome dmpolicy hprof.12345.0012.heap > for_pprof.txt
""" % (sys.argv[0]))
    sys.exit(1)

  action = sys.argv[1]
  chrome_path = sys.argv[2]
  policy_path = sys.argv[3]
  log_path = sys.argv[4]

  sys.stderr.write('parsing a policy file\n')
  policy_list = parse_policy(policy_path)

  p = re.compile('\.[0-9][0-9][0-9][0-9]\.heap')
  prefix = p.sub('', log_path)
  symbol_path = prefix + '.symbols'

  sys.stderr.write('parsing the maps file\n')
  maps_path = prefix + '.maps'
  with open(maps_path, mode='r') as maps_f:
    maps_lines = maps_f.readlines()

  # Reading buckets
  sys.stderr.write('parsing the bucket file\n')
  buckets = defaultdict(lambda: None)
  bucket_count = 0
  n = 0
  while True:
    buckets_path = '%s.%04d.buckets' % (prefix, n)
    if not os.path.exists(buckets_path):
      if n > 10:
        break
      n += 1
      continue
    sys.stderr.write('reading buckets from %s\n' % (buckets_path))
    with open(buckets_path, mode='r') as buckets_f:
      for l in buckets_f:
        words = l.split()
        st = []
        for i in range(1, len(words)):
          st.append(words[i])
        buckets[int(words[0])] = Bucket(st)
        bucket_count += 1
    n += 1

  sys.stderr.write('the number buckets: %d\n' % (bucket_count))

  log_path_list = []
  log_path_list.append(log_path)

  if action == '--csv':
    # search for the sequence of files
    n = int(log_path[len(log_path) - 9 : len(log_path) - 5])
    n += 1  # skip current file
    while True:
      p = '%s.%04d.heap' % (prefix, n)
      if os.path.exists(p):
        log_path_list.append(p)
      else:
        break
      n += 1

  logs = []
  for path in log_path_list:
    logs.append(Log(path, buckets))

  sys.stderr.write('getting symbols\n')
  read_symbols(symbol_path, maps_lines, chrome_path)

  if action == '--stacktrace':
    logs[0].dump_stacktrace(buckets)

  elif action == '--csv':
    sys.stdout.write(','.join(components))
    sys.stdout.write('\n')

    for log in logs:
      component_sizes = log.apply_policy(policy_list, buckets, logs[0].log_time)
      s = []
      for c in components:
        if c in ['hour', 'minute', 'second']:
          s.append('%05.5f' % (component_sizes[c]))
        else:
          s.append('%05.5f' % (component_sizes[c] / 1024.0 / 1024.0))
      sys.stdout.write(','.join(s))
      sys.stdout.write('\n')

  elif action == '--list':
    component_sizes = logs[0].apply_policy(
        policy_list, buckets, logs[0].log_time)
    for c in components:
      if c in ['hour', 'minute', 'second']:
        sys.stdout.write('%30s %10.3f\n' % (c, component_sizes[c]))
      else:
        sys.stdout.write('%30s %10.3f\n' % (
            c, component_sizes[c] / 1024.0 / 1024.0))

  elif action == '--expand':
    component_name = sys.argv[5]
    depth = sys.argv[6]
    logs[0].expand(policy_list, buckets, component_name, int(depth))

  elif action == '--pprof':
    if len(sys.argv) > 5:
      logs[0].dump_for_pprof(policy_list, buckets, maps_lines, sys.argv[5])
    else:
      logs[0].dump_for_pprof(policy_list, buckets, maps_lines, None)


if __name__ == '__main__':
  sys.exit(main())
