from building import *

cwd = GetCurrentDir()
src = Glob('*.c')
path = [cwd]

if GetDepend(['PKG_WS2812_USING_EXAMPLE']):
    src += Glob('example/*.c')

group = DefineGroup('ws2812', src, depend = ['PKG_USING_WS2812'], CPPPATH = path)

Return('group')
