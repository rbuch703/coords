#! /usr/bin/python3

import cairo;
import os.path;
import math;


surface = cairo.PDFSurface("map.pdf", 1000+1, 1000+1);

ctx = cairo.Context (surface);
ctx.scale(500, -500);
ctx.translate(1, -1);

ctx.set_line_width (1/2000)
ctx.set_source_rgb(0.0, 0.0, 0.0);

def lngToWebMercator(lng):
  #HALF_CIRCUMFERENCE = 20037508.34;
  HALF_CIRCUMFERENCE = 1;
  return lng * HALF_CIRCUMFERENCE / 180;

def latToWebMercator(lat):
  #HALF_CIRCUMFERENCE = 20037508.34;
  HALF_CIRCUMFERENCE = 1;
  MAX_LAT = 85.051129
  if lat > MAX_LAT: lat = MAX_LAT;
  if lat <-MAX_LAT: lat =-MAX_LAT;
  
  y = math.log(  math.tan(  (90 + lat) * math.pi / 360)) / (math.pi / 180);
  y = y * HALF_CIRCUMFERENCE / 180;
  return y;


def render(ctx, bounds, filename, depth):
    if not os.path.isfile(filename):
        return;
        
    print("rendering", filename);
    lngMin = bounds["lngMin"];
    lngMax = bounds["lngMax"];
    
    latMin = bounds["latMin"];
    latMax = bounds["latMax"];
    

    ctx.set_source_rgb(1.0 - depth/10, depth/10, 0.0);
    #ctx.set_line_width (1)

    xMin = lngToWebMercator(lngMin);
    xMax = lngToWebMercator(lngMax);
    
    yMin = latToWebMercator(latMin);
    yMax = latToWebMercator(latMax);

#    ctx.rectangle(lngMin, latMin, lngMax - lngMin, latMax - latMin);
    ctx.rectangle(xMin, yMin, xMax - xMin, yMax - yMin);
 
    ctx.stroke();

    latMid = (latMin + latMax) / 2.0;
    lngMid = (lngMin + lngMax) / 2.0;
    
    
    render( ctx, {"lngMin": lngMin, "lngMax": lngMid, "latMin": latMid, "latMax":latMax}, filename+"0", depth+1);
    render( ctx, {"lngMin": lngMid, "lngMax": lngMax, "latMin": latMid, "latMax":latMax}, filename+"1", depth+1);
    render( ctx, {"lngMin": lngMin, "lngMax": lngMid, "latMin": latMin, "latMax":latMid}, filename+"2", depth+1);
    render( ctx, {"lngMin": lngMid, "lngMax": lngMax, "latMin": latMin, "latMax":latMid}, filename+"3", depth+1);

worldBounds = {"lngMin": -180, "lngMax": 180, "latMin": -90, "latMax": 90}
render(ctx, worldBounds, "nodes/node", depth=0)

surface.finish();
surface.flush();
