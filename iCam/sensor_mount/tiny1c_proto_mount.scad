//
// Prototype Tiny1C camera mounting plate.  Designed to be glued to
// a gCore enclosure.  Holds the Tiny1C breakout rev 2, an Adafruit
// VL53L4CX breakout and a Sparkfun Spark X AHT20 breakout.
//

//
// Base plate
//
base_w = 1 + 25.4 + 1 + 31.75 + 1 + 25.4 + 1;
base_l = 1 + 25.4 + 1;
base_h = 2.0;

//
// VL53L4CX breakout dimensions
//
vlc_delta_x = 20.32;
vlc_delta_y = 12.7;
vlc_offset_x = 1 + 2.54;
vlc_offset_y = (base_l - vlc_delta_y) / 2;
vlc_so_h = 5;
vlc_so_r = 2.5;
vlc_hole_r = 0.9;

//
// Tiny1C breakout
//
t1c_delta_x = 17.4;
t1c_delta_y = 7;
t1c_offset_x = (base_w - t1c_delta_x) / 2;
t1c_offset_y = base_l - 1 - 3.325 - t1c_delta_y;
t1c_so_h = 3;
t1c_so_r = 2.25;
t1c_hole_r = 0.9;

//
// AHT20 breakout
//
a20_delta_x = 20.32;
a20_delta_y = 20.32;
a20_offset_x = base_w - 1 - 2.54 - a20_delta_x;
a20_offset_y = 1 + 2.54;
a20_so_h = 5;
a20_so_r = 3;
a20_hole_r = 1.1;



module standoff(x, y, so_r, so_h, hole_r)
{
    translate([x, y, base_h - 0.1]) {
        difference() {
            cylinder(r=so_r, h=so_h + 0.1, $fn=120);
            
            cylinder(r=hole_r, h=so_h + 0.2, $fn=120);
        }
    }
}


module base_assy()
{
    // Base
    cube([base_w, base_l, base_h]);
    
    // VL53L4CX
    standoff(vlc_offset_x, vlc_offset_y, vlc_so_r, vlc_so_h, vlc_hole_r);
    standoff(vlc_offset_x+vlc_delta_x, vlc_offset_y, vlc_so_r, vlc_so_h, vlc_hole_r);
    standoff(vlc_offset_x, vlc_offset_y+vlc_delta_y, vlc_so_r, vlc_so_h, vlc_hole_r);
    standoff(vlc_offset_x+vlc_delta_x, vlc_offset_y+vlc_delta_y, vlc_so_r, vlc_so_h, vlc_hole_r);
    
    // Tiny1C
    standoff(t1c_offset_x, t1c_offset_y, t1c_so_r, t1c_so_h, t1c_hole_r);
    standoff(t1c_offset_x+t1c_delta_x, t1c_offset_y, t1c_so_r, t1c_so_h, t1c_hole_r);
    standoff(t1c_offset_x, t1c_offset_y+t1c_delta_y, t1c_so_r, t1c_so_h, t1c_hole_r);
    standoff(t1c_offset_x+t1c_delta_x, t1c_offset_y+t1c_delta_y, t1c_so_r, t1c_so_h, t1c_hole_r);
    
    // AHT20
    standoff(a20_offset_x, a20_offset_y, a20_so_r, a20_so_h, a20_hole_r);
    standoff(a20_offset_x+a20_delta_x, a20_offset_y, a20_so_r, a20_so_h, a20_hole_r);
    standoff(a20_offset_x, a20_offset_y+a20_delta_y, a20_so_r, a20_so_h, a20_hole_r);
    standoff(a20_offset_x+a20_delta_x, a20_offset_y+a20_delta_y, a20_so_r, a20_so_h, a20_hole_r);
}


difference()
{
    base_assy();
    
    translate([(base_w - 25.4)/2, -0.1, -0.1]) {
        cube([25.4, 5, base_h + 0.2]);
    }
}
