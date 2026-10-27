// weakgcs: mast enclosure (OpenSCAD, parametric).
//
// Contents: Olimex ESP32-POE, Radiomaster Nomad (ELRS), Skyzone SteadyView V3.3,
// step-down converters. Fits a 180x180 mm bed (Bambu Lab A1 mini).
//
// Mounted orientation: back (z = 0) against the mast, y = 0 wall at the BOTTOM
// (cable gland, antennas pointing down, drain), lid at the front.
//
// Parts (render one at a time, F6, then export STL):
//   part = "box"      box, print back-down (opening up), no supports
//   part = "lid"      lid, print outer face down
//   part = "plate"    internal mounting plate (zip ties / tape), print flat
//   part = "bracket"  mast bracket for velcro straps / zip ties, print flat side down
//   part = "preview"  everything assembled + component placeholders (not for printing)
//
// Command line: openscad -D 'part="box"' -o box.stl mast_box.scad

part = "preview";

/* [Box] */
L = 130;          // outer width (X)
H = 165;          // outer height (Y, along the mast)
D = 66;           // outer depth (Z, back to lid)
wall = 3.2;
floor_t = 3;      // back wall
corner_r = 6;

/* [Lid and seal] */
lid_t = 3;
lip_h = 3;        // alignment lip inside the box
lip_w = 1.6;
lip_gap = 0.4;
groove_w = 1.8;   // for 2 mm rubber/silicone cord or a TPU-printed gasket
groove_d = 1.4;

/* [Screws: M3 with heat-set inserts] */
insert_d = 4.0;   // hole for M3 x 5 heat-set insert (check your insert's spec)
insert_h = 6;
screw_d = 3.4;
boss_d = 8.5;

/* [Openings in the bottom wall (y = 0)] */
gland_d = 15.6;       // PG9 cable gland for the twisted pair
gland_x = L / 2;
sma_d = 6.6;          // SMA bulkhead (pigtail) hole
elrs_sma_x = [L - 18, L - 38];   // Nomad: two dual-band antennas, far from the 5.8 GHz one
vrx_sma_x = [18];                // SteadyView: omni antenna
drain_d = 2.5;
drain_x = 40;
hole_z = D / 2;       // height of all bottom-wall openings

/* [Patch antenna through the lid] */
patch_sma = true;
patch_x = 34;         // lid coordinates = box coordinates (seen from the front, x mirrored)
patch_y = 125;

/* [Internal mounting plate] */
standoff_h = 6;
plate_t = 3;
standoff_pos = [[wall + 14, 30], [L - wall - 14, 30], [wall + 14, H - 22], [L - wall - 14, H - 22]];

/* [Mast bracket] */
br_w = 70;
br_l = 120;
br_base = 9;
br_v_h = 16;          // V-groove rails height
strap_w = 27;         // velcro strap up to 25 mm
strap_h = 3.5;
strap_y = [28, 92];
tie_w = 5.5;          // zip ties up to 5 mm
tie_h = 2.5;
tie_y = [50, 70];
br_screw_y = [10, 60, 110];
br_y0 = (H - br_l) / 2;   // bracket position on the box back

$fn = 48;
eps = 0.01;

// ---------------------------------------------------------------- helpers

module rrect(l, h, r) {
    translate([r, r]) offset(r = r) square([l - 2 * r, h - 2 * r]);
}

function lid_boss_pos() = [
    [wall + boss_d / 2, wall + boss_d / 2], [L - wall - boss_d / 2, wall + boss_d / 2],
    [wall + boss_d / 2, H - wall - boss_d / 2], [L - wall - boss_d / 2, H - wall - boss_d / 2]];

function bracket_screw_pos() = [for (y = br_screw_y) [L / 2, br_y0 + y]];

module bottom_hole(x, d) {
    translate([x, -1, hole_z]) rotate([-90, 0, 0]) cylinder(d = d, h = wall + 2);
}

// ---------------------------------------------------------------- box

module box() {
    difference() {
        union() {
            difference() {
                linear_extrude(D) rrect(L, H, corner_r);
                translate([0, 0, floor_t]) linear_extrude(D) offset(delta = -wall) rrect(L, H, corner_r);
            }
            for (p = lid_boss_pos()) translate([p.x, p.y, 0]) cylinder(d = boss_d, h = D);
            for (p = standoff_pos) translate([p.x, p.y, 0]) cylinder(d = boss_d, h = floor_t + standoff_h);
            for (p = bracket_screw_pos()) translate([p.x, p.y, 0]) cylinder(d = 10, h = floor_t + 4);
        }
        // gasket groove in the rim
        translate([0, 0, D - groove_d]) linear_extrude(groove_d + 1) difference() {
            offset(delta = -(wall - groove_w) / 2) rrect(L, H, corner_r);
            offset(delta = -(wall + groove_w) / 2) rrect(L, H, corner_r);
        }
        // lid screw inserts
        for (p = lid_boss_pos()) translate([p.x, p.y, D - insert_h]) cylinder(d = insert_d, h = insert_h + 1);
        // mounting plate inserts
        for (p = standoff_pos) translate([p.x, p.y, floor_t + standoff_h - insert_h + 1])
            cylinder(d = insert_d, h = insert_h);
        // bracket inserts, blind from the outside (the back stays sealed)
        for (p = bracket_screw_pos()) translate([p.x, p.y, -1]) cylinder(d = insert_d, h = insert_h + 1);
        // bottom wall
        bottom_hole(gland_x, gland_d);
        for (x = elrs_sma_x) bottom_hole(x, sma_d);
        for (x = vrx_sma_x) bottom_hole(x, sma_d);
        bottom_hole(drain_x, drain_d);
        // orientation mark on the back: arrow pointing up
        translate([L / 2, H - 25, -eps]) linear_extrude(0.6) polygon([[-6, 0], [6, 0], [0, 10]]);
    }
}

// ---------------------------------------------------------------- lid

module lid() {
    difference() {
        union() {
            linear_extrude(lid_t) rrect(L, H, corner_r);
            // alignment lip, clear of the corner bosses
            translate([0, 0, lid_t]) linear_extrude(lip_h) difference() {
                offset(delta = -(wall + lip_gap)) rrect(L, H, corner_r);
                offset(delta = -(wall + lip_gap + lip_w)) rrect(L, H, corner_r);
                for (p = lid_boss_pos()) translate(p) circle(d = boss_d + 2);
            }
        }
        for (p = lid_boss_pos()) translate([p.x, p.y, -1]) cylinder(d = screw_d, h = lid_t + 2);
        if (patch_sma) translate([patch_x, patch_y, -1]) cylinder(d = sma_d, h = lid_t + 2);
        translate([L / 2, 12, -eps]) mirror([1, 0, 0]) linear_extrude(0.6)
            text("weakgcs", size = 7, halign = "center", font = "Liberation Sans:style=Bold");
    }
}

// ---------------------------------------------------------------- mounting plate

module plate() {
    pw = L - 2 * wall - 1;
    ph = H - 2 * wall - 1;
    difference() {
        translate([wall + 0.5, wall + 0.5]) linear_extrude(plate_t) square([pw, ph]);
        // clear the lid bosses
        for (p = lid_boss_pos()) translate([p.x, p.y, -1]) cylinder(d = boss_d + 1.5, h = plate_t + 2);
        for (p = standoff_pos) translate([p.x, p.y, -1]) cylinder(d = screw_d, h = plate_t + 2);
        // zip-tie slot pairs on a grid
        for (x = [wall + 12 : 14 : L - wall - 12], y = [wall + 14 : 16 : H - wall - 10]) {
            near = min([for (p = concat(standoff_pos, lid_boss_pos())) norm([x - p.x, y - p.y])]) < 12;
            if (!near) for (dx = [-3.5, 3.5]) translate([x + dx - 1.4, y - 2.75, -1]) cube([2.8, 5.5, plate_t + 2]);
        }
    }
}

// ---------------------------------------------------------------- mast bracket

module bracket() {
    f = 6;  // flat at the bottom of the V (screw heads)
    top = br_base + br_v_h;
    difference() {
        cube([br_w, br_l, top]);
        // 90-degree V-groove along the mast
        translate([0, br_l + 1, 0]) rotate([90, 0, 0]) linear_extrude(br_l + 2) polygon([
            [br_w / 2 - f / 2 - (br_v_h + 1), top + 1], [br_w / 2 + f / 2 + (br_v_h + 1), top + 1],
            [br_w / 2 + f / 2, br_base], [br_w / 2 - f / 2, br_base]]);
        // strap and zip-tie tunnels across the bracket
        for (y = strap_y) translate([-1, y - strap_w / 2, 2.5]) cube([br_w + 2, strap_w, strap_h]);
        for (y = tie_y) translate([-1, y - tie_w / 2, 3]) cube([br_w + 2, tie_w, tie_h]);
        // M3 screws into the box back, heads in counterbores on the V floor
        for (y = br_screw_y) translate([br_w / 2, y, -1]) {
            cylinder(d = screw_d, h = top + 2);
            translate([0, 0, 5]) cylinder(d = 6.5, h = top);
        }
    }
}

// ---------------------------------------------------------------- preview

module component(pos, size, c) {
    color(c, 0.85) translate(pos) cube(size);
}

module preview() {
    z0 = floor_t + standoff_h + plate_t;
    color("DimGray") box();
    color("SteelBlue") translate([0, 0, floor_t + standoff_h]) plate();
    // placeholders: measure your parts and adjust
    component([L - wall - 3 - 45, 62, z0], [45, 80, 40], "Black");          // Radiomaster Nomad
    component([wall + 4, 108, z0], [48, 42, 16], "MidnightBlue");           // SteadyView V3.3
    component([wall + 12, 26, z0], [80, 28, 16], "ForestGreen");            // Olimex ESP32-POE
    component([wall + 4, 64, z0], [51, 26, 14], "Goldenrod");               // step-down 12 V (XL4015)
    component([wall + 4, 92, z0], [22, 17, 6], "Goldenrod");                // step-down 5 V (MP1584)
    component([wall + 30, 92, z0], [22, 17, 6], "Goldenrod");               // step-down 5 V (MP1584)
    color("Silver") translate([L / 2 - br_w / 2, br_y0 + br_l, 0]) rotate([180, 0, 0]) bracket();
    %translate([0, 0, D + lid_t]) mirror([0, 0, 1]) lid();
}

if (part == "box") box();
else if (part == "lid") lid();
else if (part == "plate") plate();
else if (part == "bracket") bracket();
else preview();
