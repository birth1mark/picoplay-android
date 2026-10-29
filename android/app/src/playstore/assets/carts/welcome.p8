pico-8 cartridge // http://www.pico-8.com
version 35
__lua__
t=0
px=64
py=64
function _init()
end
function _update()
	t+=1
	if (btn(0)) px-=1.5
	if (btn(1)) px+=1.5
	if (btn(2)) py-=1.5
	if (btn(3)) py+=1.5
	if (px<14) px=14
	if (px>114) px=114
	if (py<45) py=45
	if (py>88) py=88
end
function _draw()
	cls(1)
	rectfill(0,0,127,12,12)
	print("PICOPLAY PLAYER", 32, 4, 7)
	print("WELCOME TO PICOPLAY!", 22, 18, 11)
	print("FANTASY CONSOLE DEMO", 22, 28, 10)
	print("USE STICK/PAD TO MOVE", 20, 38, 7)
	print("TAP [+CART] TO IMPORT", 18, 96, 9)
	print("YOUR OWN .P8 GAMES", 24, 106, 9)
	local bob=sin(t/30)*2
	circfill(px, py+bob, 8, 8)
	circfill(px-3, py+bob-2, 2, 7)
	circfill(px+3, py+bob-2, 2, 7)
	pset(px-3, py+bob-2, 0)
	pset(px+3, py+bob-2, 0)
	line(px-2, py+bob+3, px+2, py+bob+3, 7)
	rectfill(0,118,127,127,0)
	print("STANDALONE HOMEBREW", 24, 120, 6)
end
