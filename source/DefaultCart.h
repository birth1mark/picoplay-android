#pragma once

#include <string>

#ifndef VER_STR
#define VER_STR "v0.0.0.0"
#endif

#ifdef PICOPLAY_PLAYSTORE_EDITION
#define PLAYSTORE_MODE_LUA "true"
#else
#define PLAYSTORE_MODE_LUA "false"
#endif

//from PicoLove api.lua
//this shouldn't really count as the bios
//this is the cart loader cart

const char * fake08DefaultCart = R"#(
pico-8 cartridge // http://www.pico-8.com
version 35
__lua__
--all themes

--start customizable per platform
cartpath = "/data/picoplay/carts/"
selectbtn = "a"
pausebtn = "start"
versionstr = ")#" VER_STR  R"#("
playstore_mode = )#" PLAYSTORE_MODE_LUA R"#(
exitbtn = "l + r"
sizebtn = "select to cycle screen sizes"
--end customizable per platform

darkenpal = {0,0,1,1,5,5,13,5,5,13,13,13,1,13,13}
darkenpal[0] = 0

-- pico8-compatible helper to find last occurrence of a character
function findlastchar(s, c)
	local lastidx = nil
	for i = 1, #s do
		if sub(s, i, i) == c then
			lastidx = i
		end
	end
	return lastidx
end
-->8
--classic

charset = "0123456789abcdefghijklmnopqrstuvwxyz_-"

stars = {}
for i = 1, 28 do
	stars[i] = {
		x = (i * 17 + 23) % 128,
		y = (i * 29 + 11) % 128,
		c = (i % 3 == 0) and 7 or ((i % 2 == 0) and 6 or 5)
	}
end

function draw_stars()
	for i = 1, #stars do
		local s = stars[i]
		local tw = (t + i * 5) % 36
		local col = s.c
		if tw < 4 then col = 10 end
		pset(s.x, s.y, col)
	end
end

function get_char_idx(c)
	for i = 1, #charset do
		if sub(charset, i, i) == c then return i end
	end
	return 1
end

function replace_char_at(str, pos, newchar)
	local pre = ""
	local post = ""
	if pos > 1 then pre = sub(str, 1, pos - 1) end
	if pos < #str then post = sub(str, pos + 1) end
	return pre .. newchar .. post
end

function classic_init()
	carts = {}
	numcarts = 0
	cidx = 0
	carttoload = ""
	t = 0
	runcmd = false

	-- BBS Online Store state
	bbs_mode = false
	bbs_items = {}
	bbs_idx = 1
	bbs_scroll = 0
	bbs_status = ""
	bbs_custom_mode = false
	bbs_custom_id = "11925"
	bbs_custom_pos = 1

	if __listcarts then
		carts = __listcarts()
		numcarts = #carts
		if numcarts > 0 then cidx = 1 end
	end

	if __bbs_listfeatured then
		bbs_items = __bbs_listfeatured()
	end
end

function classic_refreshlists()
	if __listcarts then
		carts = __listcarts()
		numcarts = #carts
		if cidx > numcarts then cidx = numcarts end
		if cidx == 0 and numcarts > 0 then cidx = 1 end
	end
	if __bbs_listfeatured then
		bbs_items = __bbs_listfeatured()
	end
end

function classic_update()
	t += 1

	if not bbs_mode then
		-- LOCAL ARCADE LIBRARY MODE
		-- Switch to BBS Store or Import Cartridge
		if btnp(4) then
			if playstore_mode then
				if __importcart then __importcart() end
				if __refreshcarts then __refreshcarts() end
				classic_refreshlists()
			else
				bbs_mode = true
				classic_refreshlists()
				bbs_status = ""
			end
			return
		end

		-- Left (0) / Up (2): Previous Cartridge (with wrap-around)
		if btnp(0) or btnp(2) then
			if numcarts > 0 then
				cidx -= 1
				if cidx < 1 then cidx = numcarts end
			end
		end

		-- Right (1) / Down (3): Next Cartridge (with wrap-around)
		if btnp(1) or btnp(3) then
			if numcarts > 0 then
				cidx += 1
				if cidx > numcarts then cidx = 1 end
			end
		end

		-- Cross (5): Launch Game!
		if btnp(5) then
			if cidx > 0 and cidx <= numcarts then
				local target = carts[cidx]
				if load and target and target ~= "" then
					load(target)
				end
			end
		end

	elseif not bbs_custom_mode then
		-- BBS STORE BROWSER MODE
		if btnp(4) then
			-- Back to local arcade
			bbs_mode = false
			classic_refreshlists()
			return
		end

		local total_items = #bbs_items + 1

		if btnp(2) then
			bbs_idx = max(1, bbs_idx - 1)
			if bbs_idx <= bbs_scroll then
				bbs_scroll = bbs_idx - 1
			end
		end
		if btnp(3) then
			bbs_idx = min(total_items, bbs_idx + 1)
			if bbs_idx > bbs_scroll + 5 then
				bbs_scroll = bbs_idx - 5
			end
		end

		if btnp(5) then
			if bbs_idx <= #bbs_items then
				local item = bbs_items[bbs_idx]
				if item.downloaded then
					load(item.file)
				else
					bbs_status = "DOWNLOADING..."
					flip()
					if __bbs_download then
						local ok = __bbs_download(item.id, item.file)
						if ok then
							item.downloaded = true
							bbs_status = "SAVED! PRESS [X] TO PLAY"
							if __refreshcarts then __refreshcarts() end
							classic_refreshlists()
						else
							bbs_status = "ERROR: BBS GET FAILED"
						end
					end
				end
			else
				-- Enter custom ID
				bbs_custom_mode = true
				bbs_custom_id = "11925"
				bbs_custom_pos = 1
				bbs_status = ""
			end
		end

	else
		-- CUSTOM ID ENTRY MODE
		if btnp(4) then
			-- Cancel back to store
			bbs_custom_mode = false
			bbs_status = ""
			return
		end

		-- Left / Right move cursor
		if btnp(0) then
			bbs_custom_pos = max(1, bbs_custom_pos - 1)
		end
		if btnp(1) then
			if bbs_custom_pos < #bbs_custom_id then
				bbs_custom_pos += 1
			elseif #bbs_custom_id < 24 then
				bbs_custom_id = bbs_custom_id .. "0"
				bbs_custom_pos = #bbs_custom_id
			end
		end

		-- Up / Down cycle character
		if btnp(2) then
			local cur_char = sub(bbs_custom_id, bbs_custom_pos, bbs_custom_pos)
			local idx = (get_char_idx(cur_char) % #charset) + 1
			bbs_custom_id = replace_char_at(bbs_custom_id, bbs_custom_pos, sub(charset, idx, idx))
		end
		if btnp(3) then
			local cur_char = sub(bbs_custom_id, bbs_custom_pos, bbs_custom_pos)
			local idx = get_char_idx(cur_char) - 1
			if idx < 1 then idx = #charset end
			bbs_custom_id = replace_char_at(bbs_custom_id, bbs_custom_pos, sub(charset, idx, idx))
		end

		-- Button 5 (Cross): Download & Run
		if btnp(5) then
			bbs_status = "DOWNLOADING..."
			flip()
			local target_file = "bbs_" .. bbs_custom_id .. ".p8.png"
			if __bbs_download then
				local ok = __bbs_download(bbs_custom_id, target_file)
				if ok then
					if __refreshcarts then __refreshcarts() end
					load(target_file)
				else
					bbs_status = "CART NOT FOUND ON BBS"
				end
			end
		end
	end
end

-- Draws a mathematically centered arcade button pill
function draw_action_btn(x, y, btn_text, label_text, btn_col)
	local bw = (#btn_text == 1) and 8 or 12
	rectfill(x, y, x + bw, y + 8, btn_col)
	rect(x, y, x + bw, y + 8, 7)
	print(btn_text, x + 3, y + 2, (btn_col == 8) and 7 or 0)
	print(label_text, x + bw + 4, y + 2, 7)
end

-- Renders an authentic physical PICO-8 cartridge card
function draw_cart_card(cx, cy, filename, is_selected)
	-- Drop shadow
	rectfill(cx + 2, cy + 2, cx + 57, cy + 39, 0)

	-- Plastic body (graphite 5)
	rectfill(cx, cy, cx + 55, cy + 37, 5)
	-- 3D Bevel
	line(cx + 1, cy, cx + 54, cy, 6)
	line(cx, cy + 1, cx, cy + 36, 6)
	line(cx + 1, cy + 37, cx + 55, cy + 37, 0)
	line(cx + 55, cy + 1, cx + 55, cy + 36, 0)

	-- Top grip ridges
	for r = 0, 4 do
		line(cx + 12 + r * 6, cy + 2, cx + 15 + r * 6, cy + 2, 0)
		line(cx + 12 + r * 6, cy + 3, cx + 15 + r * 6, cy + 3, 6)
	end

	-- Label sticker
	rectfill(cx + 5, cy + 6, cx + 50, cy + 31, 7)
	rect(cx + 4, cy + 5, cx + 51, cy + 32, 1)

	-- Iconic PICO-8 rainbow stripe across sticker top
	line(cx + 5, cy + 6, cx + 50, cy + 6, 8)
	line(cx + 5, cy + 7, cx + 50, cy + 7, 9)
	line(cx + 5, cy + 8, cx + 50, cy + 8, 10)
	line(cx + 5, cy + 9, cx + 50, cy + 9, 11)
	line(cx + 5, cy + 10, cx + 50, cy + 10, 12)

	-- Title on the sticker
	local cleanname = filename
	local lastslash = findlastchar(cleanname, "/")
	if lastslash then cleanname = sub(cleanname, lastslash + 1) end
	local lastdot = 0
	for d = 1, #cleanname do
		if sub(cleanname, d, d) == "." then lastdot = d break end
	end
	if lastdot > 0 then cleanname = sub(cleanname, 1, lastdot - 1) end
	cleanname = sub(cleanname, 1, 10)
	print(cleanname, cx + 28 - (#cleanname * 2), cy + 15, 0)

	-- Fantasy console branding
	print("PICOPLAY", cx + 12, cy + 24, 5)

	-- Bottom gold connector contacts
	rectfill(cx + 10, cy + 37, cx + 45, cy + 38, 0)
	for p = 0, 7 do
		pset(cx + 12 + p * 4, cy + 37, 10)
		pset(cx + 13 + p * 4, cy + 37, 9)
	end

	-- Animated pulsing targeting corners when selected
	if is_selected then
		local pc = (t % 20 < 10) and 10 or 7
		line(cx - 3, cy - 3, cx + 1, cy - 3, pc)
		line(cx - 3, cy - 3, cx - 3, cy + 1, pc)
		line(cx + 58, cy - 3, cx + 54, cy - 3, pc)
		line(cx + 58, cy - 3, cx + 58, cy + 1, pc)
		line(cx - 3, cy + 40, cx + 1, cy + 40, pc)
		line(cx - 3, cy + 40, cx - 3, cy + 36, pc)
		line(cx + 58, cy + 40, cx + 54, cy + 40, pc)
		line(cx + 58, cy + 40, cx + 58, cy + 36, pc)
	end
end

function classic_draw()
	cls(0)
	draw_stars()

	if not bbs_mode then
		-- ARCADE HEADER
		rectfill(0, 0, 127, 10, 1)
		line(0, 11, 127, 11, 12)
		print("PICOPLAY ARCADE", 34, 3, 7)

		-- Power Indicator LED
		local led_col = (t % 40 < 20) and 11 or 3
		circfill(120, 5, 2, led_col)
		pset(120, 5, 7)

		-- TABS (HIGH CONTRAST)
		-- Active Tab: LOCAL
		rectfill(3, 13, 61, 22, 12)
		rect(3, 13, 61, 22, 7)
		print("LOCAL CARTS", 10, 15, 0)

		-- Inactive Tab: BBS GET or IMPORT CART
		rectfill(66, 13, 124, 22, 1)
		rect(66, 13, 124, 22, 5)
		if playstore_mode then
			print("IMPORT CART", 73, 15, 6)
			print("PRESS [O] TO IMPORT", 24, 25, 10)
		else
			print("BBS STORE", 77, 15, 6)
			print("PRESS [O] TO SWITCH", 24, 25, 10)
		end

		-- CENTER CARTRIDGE DISPLAY
		local cur_cart_name = (cidx > 0 and cidx <= numcarts) and carts[cidx] or "(no carts)"
		draw_cart_card(36, 33, cur_cart_name, true)

		-- Animated Left / Right Arrows
		if numcarts > 1 then
			local lx = 16 + sin(t / 20) * 2
			local rx = 108 - sin(t / 20) * 2
			print("<", lx, 49, 12)
			print(">", rx, 49, 12)
		end

		-- INFO & METADATA CARD (Y: 77 - 107)
		rectfill(6, 77, 121, 107, 0)
		rect(6, 77, 121, 107, 1)
		rect(7, 78, 120, 106, 5)

		-- Cartridge Title
		local disptitle = cur_cart_name
		local lastslash = findlastchar(disptitle, "/")
		if lastslash then disptitle = sub(disptitle, lastslash + 1) end
		disptitle = sub(disptitle, 1, 18)
		local tx = 64 - (#disptitle * 2)
		print(disptitle, tx + 1, 82, 0)
		print(disptitle, tx, 81, 7)

		-- Badge 1: Counter Pill
		local countstr = tostr(cidx) .. " / " .. tostr(numcarts)
		rectfill(12, 93, 52, 101, 1)
		rect(12, 93, 52, 101, 5)
		print(countstr, 32 - (#countstr * 2), 95, 10)

		-- Badge 2: Type Pill
		local is_png = (sub(cur_cart_name, -4) == ".png")
		local typestr = is_png and "PNG-CART" or "P8-SRC"
		rectfill(72, 93, 115, 101, 1)
		rect(72, 93, 115, 101, 5)
		print(typestr, 94 - (#typestr * 2), 95, 11)

		-- BOTTOM ACTION / LEGEND BAR (MATHEMATICALLY CENTERED)
		rectfill(0, 113, 127, 127, 1)
		line(0, 112, 127, 112, 12)
		draw_action_btn(4, 116, "x", "PLAY", 11)
		if playstore_mode then
			draw_action_btn(42, 116, "o", "OPEN", 8)
		else
			draw_action_btn(44, 116, "o", "BBS", 8)
		end
		draw_action_btn(80, 116, "<>", "MOVE", 10)

	elseif not bbs_custom_mode then
		-- BBS STORE VIEW
		rectfill(0, 0, 127, 10, 1)
		line(0, 11, 127, 11, 11)
		print("PICOPLAY BBS STORE", 28, 3, 7)

		circfill(120, 5, 2, 11)
		pset(120, 5, 7)

		-- TABS (HIGH CONTRAST)
		-- Inactive Tab: LOCAL
		rectfill(3, 13, 61, 22, 1)
		rect(3, 13, 61, 22, 5)
		print("LOCAL CARTS", 10, 15, 6)

		-- Active Tab: BBS GET
		rectfill(66, 13, 124, 22, 11)
		rect(66, 13, 124, 22, 7)
		print("BBS STORE", 77, 15, 0)

		-- PROMPT BANNER
		print("PRESS [O] TO SWITCH", 24, 25, 10)

		-- CRT CATALOG FRAME
		rectfill(6, 30, 121, 82, 0)
		rect(6, 30, 121, 82, 1)

		-- Scrollbar Track
		line(119, 31, 119, 81, 1)
		local total_items = #bbs_items + 1
		local bar_h = 10
		local bar_y = 31 + ((bbs_idx - 1) / total_items) * (50 - bar_h)
		rectfill(118, bar_y, 120, bar_y + bar_h, 6)

		for i = 1, 5 do
			local cur = bbs_scroll + i
			local y = 32 + (i - 1) * 10
			if cur <= #bbs_items then
				local item = bbs_items[cur]
				if cur == bbs_idx then
					rectfill(8, y - 1, 116, y + 8, 1)
					rect(8, y - 1, 116, y + 8, 12)
					print(">", 10, y + 1, 10)
					print(sub(item.title, 1, 13), 17, y + 1, 7)
				else
					print(sub(item.title, 1, 13), 17, y + 1, 6)
				end

				-- Status Badge Pill
				if item.downloaded then
					rectfill(86, y, 115, y + 7, 11)
					print("PLAY", 92, y + 1, 0)
				else
					rectfill(86, y, 115, y + 7, 12)
					print("GET", 95, y + 1, 0)
				end
			elseif cur == total_items then
				if cur == bbs_idx then
					rectfill(8, y - 1, 116, y + 8, 1)
					rect(8, y - 1, 116, y + 8, 10)
					print(">", 10, y + 1, 10)
					print("[+ ENTER CART ID]", 17, y + 1, 10)
				else
					print("[+ ENTER CART ID]", 17, y + 1, 5)
				end
			end
		end

		-- DETAILS PANEL
		rectfill(6, 84, 121, 110, 0)
		rect(6, 84, 121, 110, 1)

		if bbs_idx <= #bbs_items then
			local item = bbs_items[bbs_idx]
			print("BY   : " .. sub(item.author, 1, 13), 10, 87, 7)
			print("GENRE: " .. sub(item.cat, 1, 13), 10, 95, 6)
			if bbs_status ~= "" then
				rectfill(8, 102, 119, 109, 8)
				local st_x = 64 - (#bbs_status * 2)
				print(bbs_status, st_x, 103, 7)
			elseif item.downloaded then
				print("* INSTALLED - PRESS [X] TO PLAY", 8, 103, 11)
			else
				print("+ ONLINE - PRESS [X] TO GET", 14, 103, 12)
			end
		else
			print("LEXALOFFLE BBS DOWNLOADER", 10, 87, 10)
			print("ENTER ANY BBS POST ID OR LID", 10, 95, 6)
			print("PRESS [X] TO OPEN KEYPAD", 16, 103, 12)
		end

		-- BOTTOM BAR (MATHEMATICALLY CENTERED)
		rectfill(0, 113, 127, 127, 1)
		line(0, 112, 127, 112, 11)
		draw_action_btn(4, 116, "x", "GET", 11)
		draw_action_btn(44, 116, "o", "BACK", 8)
		draw_action_btn(84, 116, "^v", "MOVE", 10)

	else
		-- CUSTOM ID ENTRY VIEW
		rectfill(0, 0, 127, 10, 1)
		line(0, 11, 127, 11, 10)
		print("BBS ID KEYPAD", 36, 3, 10)

		-- Input Box Frame
		rectfill(8, 18, 119, 56, 0)
		rect(8, 18, 119, 56, 3)
		rect(9, 19, 118, 55, 1)

		-- Monospace Character Display
		print("ID: " .. bbs_custom_id, 14, 34, 11)

		-- Animated Flashing Cursor Caret
		if t % 30 < 15 then
			local cx = 30 + (bbs_custom_pos - 1) * 4
			line(cx, 41, cx + 3, 41, 10)
		end

		-- Character Roller Wheel
		local cur_char = sub(bbs_custom_id, bbs_custom_pos, bbs_custom_pos)
		local idx = get_char_idx(cur_char)
		local prev_idx = idx - 1
		if prev_idx < 1 then prev_idx = #charset end
		local next_idx = (idx % #charset) + 1

		print(sub(charset, prev_idx, prev_idx), 108, 26, 5)
		rect(106, 32, 114, 40, 10)
		print(cur_char, 108, 34, 10)
		print(sub(charset, next_idx, next_idx), 108, 42, 5)

		-- Legend / Instructions Card
		rectfill(8, 60, 119, 108, 0)
		rect(8, 60, 119, 108, 1)
		print("[^v]  CHANGE CHARACTER", 12, 64, 7)
		print("[<>]  MOVE CURSOR / ADD", 12, 74, 7)
		print("[X]   DOWNLOAD & LAUNCH", 12, 86, 11)
		print("[O]   CANCEL / RETURN", 12, 96, 8)

		if bbs_status ~= "" then
			rectfill(8, 100, 119, 107, 8)
			local st_x = 64 - (#bbs_status * 2)
			print(bbs_status, st_x, 101, 7)
		end

		-- BOTTOM BAR
		rectfill(0, 113, 127, 127, 1)
		line(0, 112, 127, 112, 1)
		draw_action_btn(44, 116, "o", "CANCEL", 8)
	end
end

-->8
--fancy

function fancy_init()
	carts={}
	numcarts = 0
	
	showerror = false
	bioserror = ''
	errortable = {'error! press ❎ to close.'}
	if __getbioserror then
		bioserror = __getbioserror()
	end
	
	if bioserror != '' then
		showerror = true
		while true do
			if #bioserror >= 26 then
				add(errortable,sub(bioserror,1,26))
				bioserror = sub(bioserror,27,-1)
			else
				add(errortable,bioserror)
				break
			end
		end
	end
	
	if __getsetting then
			bgcolor = __getsetting('p8_bgcolor')
			textcolor = __getsetting('p8_textcolor')
	else
		bgcolor=1
		textcolor = 7
	end
	
	
	cidx=0
	carttoload = ""
	t=0
	linebuffer=""
	
	
	runcmd=false
	
	cpos = 0
	dcpos = 0
	loadedimages = {}
	if __listcarts then
		cartnames = __listcarts()
		numcarts = #cartnames
	else
		--cartnames = {'cart1.p8','cart2.p8.png','thirdcart.p8','cart4.p8','5cart.p8','c6.p8','theseventhcart.p8.png'}
		cartnames = {}
		numcarts = #cartnames
	end
	
	loading = true
	
	loadtotal = #cartnames
	loadnumber = 1
	

	


end

function fancy_updatecarts()
	for i,v in ipairs(carts) do
		if v.id >= cpos -1 and v.id <= cpos+1 then
			v.visible = true
			
			if loadedimages[v.sprite] ~= v.name then
				loadedimages[v.sprite] = v.name
				if __loadlabel then
					__loadlabel(v.name,true,v.sprite*16)
				end
			end
		
		else
			v.visible = false
		end
		if v.id == cpos then
			carttoload = v.name
		end
	end
end

function fancy_update()
	if not loading then
		if showerror then
			if btnp(❎) then
				showerror = false
			end
		elseif #carts > 0 then
			if btnp(➡️) then
				cpos = (cpos + 1)%numcarts
				fancy_updatecarts()
			end
			if btnp(⬅️) then
				cpos = (cpos - 1)%numcarts
				fancy_updatecarts()
			end
			dcpos = (cpos*0.5 + dcpos*1.5) / 2
			
			if btnp(❎) then
				if load and carttoload then
					load(carttoload)
				end
			end
		end
	else
		
		
		local i = loadnumber
		local v = cartnames[i]
		if loadnumber <= loadtotal then
			local newcart = {}
			newcart.name = v
			newcart.id = i - 1
			newcart.sprite = newcart.id%4
			newcart.text = {"a","e"}
			
			if __getlualine then
				local l = __getlualine(v,0)
				if sub(l,1,2) == '--' then
					l = sub(l,3,#l)
					if sub(l,1,1) == ' ' then
						l = sub(l,2,#l)
					end
					newcart.text[2] = l
					newcart.text[1] = v
				else
					newcart.text[1] = v
					newcart.text[2] = ''
				end
			else
				newcart.text[1] = v
				newcart.text[2] = 'name'
			end
			printh(newcart.text[1])
			
			add(carts,newcart)
			
			loadnumber += 1
		else
			fancy_updatecarts()
			loading = false
		end
		
	end
	
end

function fancy_drawcart(v)
	local cartx = (v.id-dcpos+1)*44+4
	local carty = 60
	local textx = cartx +16
	local texty = -6
	carty+= sin((v.id-dcpos+1)/4)*10
	texty-= sin((v.id-dcpos+1)/4)*24
	
	for li = 1,2 do
		local drawline = true
		if li == 2 then
			if v.text[1] == v.text[2] then
				drawline = false
			end
		end
		
		if drawline then
			print(v.text[li],textx-(#v.text[li])*2,texty+ li*8,textcolor)
		end
		
	end
	
	--base
	rectfill(cartx - 4,
	carty-6,
	cartx+35,
	carty+42,
	5)
	line(cartx-4,carty+43,cartx+34,carty+43,5)
	line(cartx-4,carty+44,cartx+33,carty+44,5)
	
	--border
	line(cartx-4,carty-7,cartx+35,carty-7,0)
	line(cartx-5,carty-6,cartx-5,carty+44,0)
	line(cartx+36,carty-6,cartx+36,carty+42,0)
	line(cartx+34,carty+44,cartx+36,carty+42,0)
	line(cartx-4,carty+45,cartx+33,carty+45,0)
	
	--label
	rect(cartx-1,carty-1,cartx+32,carty+32,0)
	spr(v.sprite*4,
		cartx,
		carty,
	4,4)
	
	--bottom label
	rect(cartx-1,carty+35,cartx+32,carty+42,0)
	line(cartx+1,carty+37,cartx+#v.name,carty+37,7)
	line(cartx+1,carty+39,cartx+(#v.name*3)%20+2,carty+39,7)
	
	line(cartx+1,carty+41,cartx+10,carty+41,6)
	--markings
	line(cartx-1,carty-3,cartx+2,carty-3,7)
	pset(cartx+4,carty-4,7)
	line(cartx+20,carty-4,cartx+32,carty-4,6)

	
end

function fancydrawmain()
	rectfill(0,0,128,128,bgcolor)
	if not loading then
		if #carts > 0 then
			for i,v in ipairs(carts) do
				fancy_drawcart(v)
			end
			--rect(44,44,83,83,7)
			--print(carts[cpos+1].name,0,0,textcolor)
			rect(0,120,127,127,textcolor)
			
			local barx = dcpos/(numcarts-1)*128-3
			print('◆',barx,121,textcolor)
			print('◆',barx,122,textcolor)
		else
			print('no carts found!',34,50,textcolor)
			local loctext = "place p8 carts in " .. cartpath
			print(loctext,64-(#loctext)*2,60,textcolor)
		end
	else
		color(textcolor)
		print('loading carts',37,50)
		local lstr = loadnumber .. ' / ' .. loadtotal
		print(lstr,64 - (#lstr*2),60)
	end
end


function fancy_draw()
	cls()
	clip()
	pal()
	palt(0,false)
	fancydrawmain()
	
	
	
	if showerror then
		rect(9,9,118,118,7)
		pal(darkenpal)
		clip(10,10,108,108)
		fancydrawmain()
		pal()
		for i,v in ipairs(errortable) do
			print(v,12,i*8+4,7)
		end
		
		
	end
end


-->8
--list

function list_init()
	carts={}
	numcarts = 0
	
	showerror = false
	bioserror = ''
	errortable = {'error! press ❎ to close.'}
	if __getbioserror then
		bioserror = __getbioserror()
	end
	
	if bioserror != '' then
		showerror = true
		while true do
			if #bioserror >= 26 then
				add(errortable,sub(bioserror,1,26))
				bioserror = sub(bioserror,27,-1)
			else
				add(errortable,bioserror)
				break
			end
		end
	end
	
	if __getsetting then
			bgcolor = __getsetting('p8_bgcolor')
			textcolor = __getsetting('p8_textcolor')
	else
		bgcolor=1
		textcolor = 7
	end
	
	if bgcolor == 1 or bgcolor == 0 then
		bgcolor = 13
	end
	
	
	cidx=0
	carttoload = ""
	t=0
	linebuffer=""
	
	
	runcmd=false
	
	cpos = 0
	dcpos = 0
	
	if __listcarts then
		cartnames = __listcarts()
		numcarts = #cartnames
	else
		--cartnames = {'cart1.p8','cart2.p8.png','thirdcart.p8','cart4.p8','5cart.p8','c6.p8','theseventhcart.p8.png'}
		cartnames = {}
		numcarts = #cartnames
	end
	
	loading = true
	
	loadtotal = #cartnames
	loadnumber = 1
	

	


end

function list_updatecarts()
	for i,v in ipairs(carts) do
		if v.id == cpos and not v.visible then
			v.visible = true
			
			if __loadlabel then
				__loadlabel(v.name,false,0)
			end
		
		else
			v.visible = false
		end
		if v.id == cpos then
			carttoload = v.name
		end
	end
end

function list_update()
	if not loading then
		if showerror then
			if btnp(❎) then
				showerror = false
			end
		elseif #carts > 0 then
			if btnp(⬇️) then
				cpos = (cpos + 1)%numcarts
				list_updatecarts()
			end
			if btnp(⬆️) then
				cpos = (cpos - 1)%numcarts
				list_updatecarts()
			end
			if btnp(⬅️) then
				cpos = (cpos - 7)%numcarts
				list_updatecarts()
			end
			if btnp(➡️) then
				cpos = (cpos + 7)%numcarts
				list_updatecarts()
			end
			dcpos = (cpos*0.5 + dcpos*1.5) / 2
			
			if btnp(❎) then
				if load and carttoload then
					load(carttoload)
				end
			end
		end
	else
		
		
		local i = loadnumber
		local v = cartnames[i]
		if loadnumber <= loadtotal then
			local newcart = {}
			newcart.name = v
			newcart.id = i - 1
			newcart.sprite = newcart.id%4
			newcart.text = {"a","e"}
			newcart.visible = false
			
			if __getlualine then
				local l = __getlualine(v,0)
				if sub(l,1,2) == '--' then
					l = sub(l,3,#l)
					if sub(l,1,1) == ' ' then
						l = sub(l,2,#l)
					end
					newcart.text[2] = l
					newcart.text[1] = v
				else
					newcart.text[1] = v
					newcart.text[2] = ''
				end
			else
				newcart.text[1] = v
				newcart.text[2] = 'name'
			end
			
			add(carts,newcart)
			
			loadnumber += 1
		else
			list_updatecarts()
			loading = false
		end
		
	end
	
end



function listdrawbg()
	rectfill(0,0,128,128,bgcolor)
	spr(0,0,0,16,16)
	clip(10,72,108,56)
	pal(darkenpal)
	spr(0,0,0,16,16)
	pal()
end


function list_draw()
	cls()
	
	
	
	
	if not loading then
		clip()
		pal()
		palt(0,false)
		listdrawbg()
		if showerror then
			rect(9,9,118,118,7)
			pal(darkenpal)
			clip(10,10,108,108)
			spr(0,0,0,16,16)
			pal()
			for i,v in ipairs(errortable) do
				print(v,12,i*8+4,7)
			end
			
			
		else
			print(cartpath,12,74,7)
			line(12,81,116,81,6)
			
			
			if #carts > 0 then
				clip(12,82,106,46)
				for i,v in ipairs(carts) do
					local drawpos = (i - 1 - dcpos)*7+0.5
					if cpos + 1 == i then
						
						local hoffset = 16 + #v.text[1]*4
						
						rectfill(12,99 + drawpos,hoffset,105+drawpos,bgcolor)
						
						rectfill(hoffset,99+drawpos,hoffset + (#v.text[2])*4+6,105+drawpos,textcolor)
						
						print(v.text[2],hoffset+4,100+drawpos,bgcolor)
						
						rect(0,98+drawpos,128,106+drawpos,0)
						
						color(textcolor)
					else
						color(7)
					end
					print(v.text[1],13,100+drawpos)
				end
			else
				print('no carts found!',14,86,7)
			end
		end
	else
		rectfill(0,0,128,128,bgcolor)
		color(textcolor)
		print('loading carts',37,50)
		local lstr = loadnumber .. ' / ' .. loadtotal
		print(lstr,64 - (#lstr*2),60)
	end
end


-->8
--runner

inits = {classic_init,fancy_init,list_init}
function _init()
	--disabling menu styles - they are currently broken
	--if __getsetting then
	--	theme = __getsetting('menustyle')
	--else
	--	theme = 2
	--end
	--theme = theme % #inits --don't load a nonexistant menu style
	
	theme = 0
	
	inits[theme+1]()
end

updates = {classic_update,fancy_update,list_update}
function _update60()
	updates[theme+1]()
end

draws = {classic_draw,fancy_draw,list_draw}
function _draw()	
	draws[theme+1]()
end



__gfx__
0123456789abcdef3333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888888888888cccccccccccccccccccccccccccccccc
0001155d55ddd1dd3333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888888888888cccccccccccccccccccccccccccccccc
33333333333377773333333333333333ddddddddddddddd77ddddddddddddddd88888888888888777788888888888888cccccccccc77777777cccccccccccccc
33333333333773337333333333333333ddddddddddddddd77ddddddddddddddd88888888888887788778888888888888cccccccccc7ccccccc7ccccccccccccc
33333333333733333773333333333333ddddddddddddddd77ddddddddddddddd88888888888877888887888888888888cccccccccccccccccc7ccccccccccccc
33333333337333333373333333333333dddddddddddddd7d7ddddddddddddddd88888888888778888888788888888888cccccccccccccccccc7ccccccccccccc
33333333337333333337333333333333ddddddddddddd7dd7ddddddddddddddd88888888888788888888788888888888cccccccccccccccccc7ccccccccccccc
33333333773333333333733333333333ddddddddddddd7dd7ddddddddddddddd88888888888888888888788888888888cccccccccccccccccc7ccccccccccccc
33333333773333333333373333333333dddddddddddd7ddd7ddddddddddddddd88888888888888888888788888888888cccccccccccccccccc7ccccccccccccc
33333337773333333333337333333333ddddddddddd77ddd7ddddddddddddddd88888888888888888888788888888888cccccccccccccccccc7ccccccccccccc
33333373733333333333337333333333dddddddddddddddd7ddddddddddddddd88888888888888888888788888888888cccccccccccccccc77cccccccccccccc
33333373333333333333337333333333dddddddddddddddd7ddddddddddddddd88888888888888888888788888888888ccccccccccccccc7777ccccccccccccc
33333373333333333333333733333333dddddddddddddddd7ddddddddddddddd88888888888888888888788888888888ccccccccccccccc77c7ccccccccccccc
33333337733333333333333733333333dddddddddddddddd7ddddddddddddddd88888888888888888888788888888888ccccccccccccccc7cc7ccccccccccccc
33333333373333333333337333333333ddddddddddddddd77ddddddddddddddd88888888888888888888788888888888cccccccccccccccccc7ccccccccccccc
33333333337733333333773333333333ddddddddddd77777777ddddddddddddd88888888888888888887888888888888cccccccccccccccccc7ccccccccccccc
33333333333773333777333333333333dddddddddddddddddd7777dddddddddd88888888888888888887888888888888ccccccccccccc77cc7cccccccccccccc
33333333333337777733333333333333dddddddddddddddddddddddddddddddd88888888888888888887888888888888ccccccccccccccc777cccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888878888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888877777788888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888877777888888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888777888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888888777888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888778888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888888888888cccccccccccccccccccccccccccccccc
33333333333333333333333333333333dddddddddddddddddddddddddddddddd88888888888888888888888888888888cccccccccccccccccccccccccccccccc
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777770777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777770777777077777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777770777777077777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777770777777077777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777770777770077777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777770777770000777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777770000000777077777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777770777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777770777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777770777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777770777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777770777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777770777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777777777777777777777777777777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777077777077007077777000000007777007777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77000077077077077077000000000077077077077000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77777077777077770077770077777077077077777000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77000077077077077077000000000077077077077000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
77000077077077007077777000000077770077770000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

)#";