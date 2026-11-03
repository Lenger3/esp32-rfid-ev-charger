// ============================================================================
// PLAKET KUTUSU (v2) — OTOMATIK URETILDI  tools/gen_box_pcb.py
// Dis: 124 x 124 x 48 mm (kapak dahil)
// Sadece 100x100 plaket. Ekran/buton/RC522 buyuk kutuya gider.
// ============================================================================

W = 124;
HD = 124;
BODY_H = 45;
WALL = 3;
LID_T = 3;

INT_W = 118;
INT_H = 118;

PCB_X0 = 12;
PCB_Y0 = 12;
PCB_Z0 = 11;
PCB_TOP = 12.6;

CABLE_Z0 = 14;
CABLE_W = 38;
CABLE_H = 14;

$fn = 48;

module body() {
  difference() {
    union() {
      difference() {
        cube([W, HD, BODY_H]);
        translate([WALL, WALL, WALL]) cube([INT_W, INT_H, BODY_H - WALL + 1]);
      }
      // plaket standoff'lari
      translate([16, 16, 2.5]) difference(){ cylinder(h=8.5, d=6, $fn=32); translate([0,0,-0.1]) cylinder(h=8.7, d=2.7, $fn=24); }
      translate([108, 16, 2.5]) difference(){ cylinder(h=8.5, d=6, $fn=32); translate([0,0,-0.1]) cylinder(h=8.7, d=2.7, $fn=24); }
      translate([16, 108, 2.5]) difference(){ cylinder(h=8.5, d=6, $fn=32); translate([0,0,-0.1]) cylinder(h=8.7, d=2.7, $fn=24); }
      translate([108, 108, 2.5]) difference(){ cylinder(h=8.5, d=6, $fn=32); translate([0,0,-0.1]) cylinder(h=8.7, d=2.7, $fn=24); }
      // govde vidalari icin kulaklar
      translate([6.75, 6.75, 2.5]) cylinder(h=42.5, d=6.5, $fn=32);
      translate([117.25, 6.75, 2.5]) cylinder(h=42.5, d=6.5, $fn=32);
      translate([6.75, 117.25, 2.5]) cylinder(h=42.5, d=6.5, $fn=32);
      translate([117.25, 117.25, 2.5]) cylinder(h=42.5, d=6.5, $fn=32);
      // buyuk kutuya montaj kulaklari
      translate([-10, 47, 0]) cube([10, 30, 4]);
      translate([124, 47, 0]) cube([10, 30, 4]);
      // kablo baglama tirnaklari
      translate([50.5, 113, 2.5]) difference(){ cylinder(h=14.5, d=5, $fn=24); translate([0,0,10]) cylinder(h=6, d=3, $fn=16); }
      translate([73.5, 113, 2.5]) difference(){ cylinder(h=14.5, d=5, $fn=24); translate([0,0,10]) cylinder(h=6, d=3, $fn=16); }
    }
    // vida delikleri
    translate([6.75, 6.75, -1]) cylinder(h=47, d=2.8, $fn=24);
    translate([117.25, 6.75, -1]) cylinder(h=47, d=2.8, $fn=24);
    translate([6.75, 117.25, -1]) cylinder(h=47, d=2.8, $fn=24);
    translate([117.25, 117.25, -1]) cylinder(h=47, d=2.8, $fn=24);
    // montaj kulagi delikleri
    translate([-5, 62, -1]) cylinder(h=6, d=4.4, $fn=24);
    translate([129, 62, -1]) cylinder(h=6, d=4.4, $fn=24);
    // KABLO CIKISI (+Y duvar)
    translate([W/2 - CABLE_W/2, HD - WALL - 1, CABLE_Z0])
      cube([CABLE_W, WALL + 2, CABLE_H]);
    // USB gecisi (-X duvar)
    translate([-1, 28.5, 13.5])
      cube([WALL + 2, 19, 13]);
    // 12V giris deligi (+X duvar)
    translate([120, 54, 21])
      rotate([0, 90, 0]) cylinder(h=5, d=10, $fn=48);
    // yan havalandirma (-Y duvar)
    translate([17.5, -1, 7]) cube([3, 5, 26]);
    translate([35.5, -1, 7]) cube([3, 5, 26]);
    translate([53.5, -1, 7]) cube([3, 5, 26]);
    translate([71.5, -1, 7]) cube([3, 5, 26]);
    translate([89.5, -1, 7]) cube([3, 5, 26]);
  }
}

module lid() {
  difference() {
    union() {
      translate([0, 0, BODY_H]) cube([W, HD, LID_T]);
      // LIP dudak (ic bosluga oturur)
      translate([WALL + 0.8, WALL + 0.8, BODY_H - 2.6])
        difference() {
          cube([INT_W - 1.6, INT_H - 1.6, 2.65]);
          translate([1.6, 1.6, -0.5]) cube([INT_W - 4.8, INT_H - 4.8, 4]);
        }
    }
    // govde vida delikleri
    translate([6.75, 6.75, 44.9]) cylinder(h=5, d=3.4, $fn=24);
    translate([117.25, 6.75, 44.9]) cylinder(h=5, d=3.4, $fn=24);
    translate([6.75, 117.25, 44.9]) cylinder(h=5, d=3.4, $fn=24);
    translate([117.25, 117.25, 44.9]) cylinder(h=5, d=3.4, $fn=24);
    // kapak havalandirmasi (5 kanal)
    translate([46, 19.5, 44]) cube([32, 3, 5]);
    translate([46, 37.5, 44]) cube([32, 3, 5]);
    translate([46, 55.5, 44]) cube([32, 3, 5]);
    translate([46, 73.5, 44]) cube([32, 3, 5]);
    translate([46, 91.5, 44]) cube([32, 3, 5]);
  }
}

part = "both";
if (part == "body") body();
else if (part == "lid") lid();
else { body(); translate([W + 12, 0, 0]) lid(); }
