#! /usr/bin/python3

import cairo;
import os.path;

surface = cairo.PDFSurface("map.pdf", 360*10+1, 180*10+1);

ctx = cairo.Context (surface);
ctx.scale(10, -10);
ctx.translate(180, -90);

ctx.set_line_width (20 / 180.0)
ctx.set_source_rgb(0.0, 0.0, 0.0);


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
    ctx.rectangle(lngMin, latMin, lngMax - lngMin, latMax - latMin);
    ctx.stroke();
    
    latMid = (latMin + latMax) / 2.0;
    lngMid = (lngMin + lngMax) / 2.0;
    
    render( ctx, {"lngMin": lngMin, "lngMax": lngMid, "latMin": latMid, "latMax":latMax}, filename+"0", depth+1);
    render( ctx, {"lngMin": lngMid, "lngMax": lngMax, "latMin": latMid, "latMax":latMax}, filename+"1", depth+1);
    render( ctx, {"lngMin": lngMin, "lngMax": lngMid, "latMin": latMin, "latMax":latMid}, filename+"2", depth+1);
    render( ctx, {"lngMin": lngMid, "lngMax": lngMax, "latMin": latMin, "latMax":latMid}, filename+"3", depth+1);

#ctx.set_line_width (1)
#ctx.move_to(-180, -90);
#ctx.line_to(180, 90);
#ctx.stroke();

worldBounds = {"lngMin": -180, "lngMax": 180, "latMin": -90, "latMax": 90}
render(ctx, worldBounds, "nodes/node", depth=0)

surface.finish();
surface.flush();
