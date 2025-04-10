//
// Spacer for Infiray Tiny1C breakout board
//

difference()
{
    cube([4.19, 12.95, 0.7]);
    
    translate([(4.19/2), 2.63, -0.1]) {
        cylinder(r=1, h=1, $fn=120);
    }
    
    translate([(4.19/2), 9.63, -0.1]) {
        cylinder(r=1, h=1, $fn=120);
    }
}