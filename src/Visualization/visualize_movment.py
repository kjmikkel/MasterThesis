#!/usr/bin/python
#-*- coding: utf-8 -*-

# visualize_movement.py: Script to visulize the movement of nodes and print them to a svg file.
#Copyright (C) 2011 Mikkel Kjær Jensen (kjmikkel@gmail.com)
#
#This program is free software; you can redistribute it and/or
#modify it under the terms of the GNU General Public License
#as published by the Free Software Foundation; either version 2
#of the License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

import svgfig
from svgfig import *
import os, math, colorsys, webcolors

dir_list = ["test_movement"]
colour_list = webcolors.css3_names_to_hex.keys()

for fdir in dir_list:
    fig = Fig()
    min_x = sys.float_info.max
    min_y = sys.float_info.max

    max_x = sys.float_info.min
    max_y = sys.float_info.min

    i = 0
    for filename in os.listdir(fdir):
        fig2 = Fig()
        f = open(fdir + '/' + filename)
        lines = f.readlines()
        
        old_x = -1
        old_y = -1
        
        for line in lines:
            split = line.split(' ')
            x = float(split[0])
            y = float(split[1])
            
            min_x = min(min_x, x)
            max_x = max(max_x, x)
            
            min_y = min(min_y, y)
            max_y = max(max_y, y)
             
            if old_x > 0 or old_y > 0:
                l = Line(old_x, old_y, x, y, stroke=colour_list[i])                
                fig2.d.append(l)
                
            old_x = x
            old_y = y
        
        i += 1
        fig.d.append(fig2)
        
    svgfig._canvas_defaults['width'] = "800px"
    svgfig._canvas_defaults['height'] = "800px"
    svgfig._canvas_defaults['viewBox'] = "0 0 " + str(max_x) + " " + str(max_y)
    
    
    s = SVG("top_level")  
    s.append(fig.SVG())
    s.save(fdir + ".svg")
