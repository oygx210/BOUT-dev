from __future__ import print_function

# BOUT++ Library - Write fluid simulations in curviilinear geometry
# Copyright (C) 2016, 2017, 2018 David Schwörer
#
# Contact: Ben Dudson, bd512@york.ac.uk
#
# This file is part of BOUT++.
#
# BOUT++ is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# BOUT++ is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with BOUT++.  If not, see <http://www.gnu.org/licenses/>.


# Define some general constants, functions, ...

def warn():
    print("\n// This file is auto-generated - do not edit!")

def license():
    return """/********************************************************
 * BOUT++ Library - Write fluid simulations in curviilinear geometry
 * Copyright (C) 2016, 2017, 2018 David Schwörer
 *
 * Contact: Ben Dudson, bd512@york.ac.uk
 *
 * This file is part of BOUT++.
 *
 * BOUT++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BOUT++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with BOUT++.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************/
"""

# simple debug logger - easy to disable in a single point
debug_enabled = False  # True


def debug(*args, enable=None, **kwargs):
    global debug_enabled
    if enable is not None:
        debug_enabled = enable
    if debug_enabled:
        import sys
        print(*args, file=sys.stderr, **kwargs)

fields = ['Field3D', 'Field2D']
dirs = dict()
dirs['Field3D'] = ['x', 'y', 'z']
dirs['Field2D'] = ['x', 'y']
dir_number = {'x': 0, 'y': 1, "z": 2}
perp_dir = dict()
for field in fields:
    perp_dir[field] = dict()
    for d in dirs[field]:
        perp_dir[field][d] = []
        for d2 in dirs[field]:
            if d != d2:
                perp_dir[field][d].append(d2)

off_diff = dict()
for stag in ['on', 'off', 'norm']:
    curr = dict()
    if stag == 'off':
        curr = {'ppp': 'ppp',
                'pp': 'pp',
                'p': 'p',
                'm': 'c',
                'mm': 'm',
                'mmm': 'mm'}
    elif stag == 'on':
        curr = {'ppp': 'pp',
                'pp': 'p',
                'p': 'c',
                'm': 'm',
                'mm': 'mm',
                'mmm': 'mmm'}
    elif stag == 'norm':
        curr = {'ppp': 'ppp',
                'pp': 'pp',
                'p': 'p',
                'c': 'c',
                'm': 'm',
                'mm': 'mm',
                'mmm': 'mmm'}
    for d in curr:
        curr[d] = curr[d] + '()'
        if curr[d] == 'mmm()':
            curr[d] = 'm(3)'
            if curr[d] == 'ppp()':
                curr[d] = 'p(3)'
    if stag == 'on':
        for i in range(1, 6):
            curr['m%d' % i] = 'm(%d)' % i
            curr['p%d' % i] = 'p(%d)' % (i - 1)
    if stag == 'off':
        for i in range(1, 6):
            curr['m%d' % i] = 'm(%d)' % (i - 1)
            curr['p%d' % i] = 'p(%d)' % (i)
    off_diff[stag] = curr

diff2 = {'mm()': -2,
         'm()': -1,
         'c()':  0,
         'p()':  1,
         'pp()':  2}
for i in range(10):
    diff2["m(%d)" % i] = -i
    diff2["p(%d)" % i] = i
numGuards = {'DDX_C2': 1,
             'DDX_C4': 2,
             'DDX_U1': 1,
             'DDX_U2': 2,
             'DDX_U4': 2,
             'DDX_CWENO2': 1,
             'DDX_WENO3': 2,
             'DDX_S2': 2,
             'DDX_NND': 2,
             'DDX_C2_stag': 1,
             'DDX_C4_stag': 2,
             'DDX_U1_stag': 1,
             'DDX_U2_stag': 2,
             'DDX_U4_stag': 2,
             'DDX_WENO2_stag': 1,
             'DDX_WENO3_stag': 2,
             'DDX_S2_stag': 2,
             'DDX_NND_stag': 2,
             'D2DX2_C2': 1,
             'D2DX2_C4': 2,
             'D2DX2_U1': 1,
             'D2DX2_U2': 2,
             'D2DX2_U4': 2,
             'D2DX2_WENO2': 1,
             'D2DX2_WENO3': 2,
             'D2DX2_S2': 2,
             'D2DX2_NND': 2,
             'D2DX2_C2_stag': 1,
             'D2DX2_C4_stag': 2,
             'D2DX2_U1_stag': 1,
             'D2DX2_U2_stag': 2,
             'D2DX2_U4_stag': 2,
             'D2DX2_WENO2_stag': 1,
             'D2DX2_WENO3_stag': 2,
             'D2DX2_S2_stag': 2,
             'D2DX2_NND_stag': 2,
             'VDDX_C4': 2,
             }


def duplicates(a):
    import collections
    dup = [item for item, count in list(
        collections.Counter(a).items()) if count > 1]
    if dup:
        import sys
        print("found duplicates:", file=sys.stderr)
        print(dup, file=sys.stderr)
        print("in", file=sys.stderr)
        print(a, file=sys.stderr)
        raise "Exit"


def ASSERT(a):
    if a != True:
        raise AssertionError("Assertion failed")


class UniqueList(list):

    def append(self, item):
        for old in self:
            if (item == old):
                raise RuntimeError("Trying to insert an item that exists"
                                   "\nOld items: %s\nNew item: %s" % (old, item))
        super().append(item)


class braces(object):

    def __init__(self, string="", end=""):
        self.end = end
        print("%s {" % string)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        print("}" + self.end)
