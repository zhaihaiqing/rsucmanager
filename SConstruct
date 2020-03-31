# File      : SConstruct.py
#             Building Script for User Applications
# This file is part of RT-Thread RTOS
# COPYRIGHT (C) 2006 - 2015, RT-Thread Development Team
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License along
#  with this program; if not, write to the Free Software Foundation, Inc.,
#  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Change Logs:
# Date           Author       Notes
# 2015-02-07     Bernard      The firstly version
# 2018-04-15     Bernard      Add the build option for library
#

import os

from tools.ua import BuildApplication
from tools.ua import BuildLibrary

AddOption('--app',
  dest='app',
  nargs=1, type='string',
  action='store',
  metavar='DIR',
  help='application to be built')

AddOption('--lib',
  dest='lib',
  nargs=1, type='string',
  action='store',
  metavar='DIR',
  help='library to be built')

app = GetOption('app')
lib = GetOption('lib')

if app == None and lib == None:
  print("none application or library, please use ")
  print("    scons --app=your_app")
  print("    scons --lib=your_lib")
  exit(0)

else:
  if app:
    BuildApplication(app + '/' + app + '.mo', app +'/SConscript')

  if lib:
    BuildLibrary(lib + '/' + lib + '.so', lib +'/SConscript')
