// ============================================================================
// EV Sarj Kutusu — OTOMATIK URETILDI (tools/gen_box.py)
// Dis olcu: 140 x 170 x 62 mm
// Malzeme onerisi: PETG, 3 perde, %25 dolgu
// ============================================================================

W = 140;
H = 170;
D = 62;
WALL = 3;
LID_T = 3;
BODY_D = 59;

INT_W = 134;
INT_H = 164;
INT_D = 56;
LOWER_H = 37;
DIV_T = 3;

$fn = 48;

module body() {
  difference() {
    union() {
      // 1) kabuk: dis kutudan ic bosluk cikarilir (on yuz acik)
      difference() {
        cube([W, H, BODY_D]);
        translate([WALL, WALL, WALL]) cube([INT_W, INT_H, BODY_D - WALL + 1]);
      }
      // 2) ayirici (alt bolme: 12V PSU) — kabuk kesildikten SONRA eklenir
      translate([WALL, WALL + LOWER_H, WALL])
        cube([INT_W, DIV_T, BODY_D - WALL]);
      // 3) plaket standoff'lari
      translate([20.5, 54.5, 3]) difference(){ cylinder(h=8, d=6, $fn=32); translate([0,0,-0.1]) cylinder(h=8.2, d=2.7, $fn=24); }
      translate([113.5, 54.5, 3]) difference(){ cylinder(h=8, d=6, $fn=32); translate([0,0,-0.1]) cylinder(h=8.2, d=2.7, $fn=24); }
      translate([20.5, 147.5, 3]) difference(){ cylinder(h=8, d=6, $fn=32); translate([0,0,-0.1]) cylinder(h=8.2, d=2.7, $fn=24); }
      translate([113.5, 147.5, 3]) difference(){ cylinder(h=8, d=6, $fn=32); translate([0,0,-0.1]) cylinder(h=8.2, d=2.7, $fn=24); }
      // 4) vida kulaklari
    translate([11, 11, 3]) cylinder(h=56, d=8, $fn=32);
    translate([129, 11, 3]) cylinder(h=56, d=8, $fn=32);
    translate([11, 159, 3]) cylinder(h=56, d=8, $fn=32);
    translate([129, 159, 3]) cylinder(h=56, d=8, $fn=32);
      // 5) PSU tutma tirnaklari (alt bolme)
      translate([19, 4, 3]) cube([6, 6, 6]);
      translate([115, 4, 3]) cube([6, 6, 6]);
      translate([19, 33, 3]) cube([6, 6, 6]);
      translate([115, 33, 3]) cube([6, 6, 6]);
    }
    // vida delikleri
    translate([11, 11, -1]) cylinder(h=61, d=2.8, $fn=24);
    translate([129, 11, -1]) cylinder(h=61, d=2.8, $fn=24);
    translate([11, 159, -1]) cylinder(h=61, d=2.8, $fn=24);
    translate([129, 159, -1]) cylinder(h=61, d=2.8, $fn=24);
    // kablo rakorlari (alt yuz)
    translate([22, -1, 31]) rotate([-90,0,0]) cylinder(h=5, d=16.2, $fn=48);
    translate([67, -1, 31]) rotate([-90,0,0]) cylinder(h=5, d=16.2, $fn=48);
    translate([110, -1, 31]) rotate([-90,0,0]) cylinder(h=5, d=16.2, $fn=48);
    // bolme kablosu gecisi
    translate([40, 39, 31]) rotate([-90,0,0]) cylinder(h=5, d=12, $fn=48);
    translate([94, 39, 31]) rotate([-90,0,0]) cylinder(h=5, d=12, $fn=48);
    // ust havalandirma
    translate([16.5, 166.5, 10]) cube([3, 4, 40]);
    translate([32.5, 166.5, 10]) cube([3, 4, 40]);
    translate([48.5, 166.5, 10]) cube([3, 4, 40]);
    translate([64.5, 166.5, 10]) cube([3, 4, 40]);
    translate([80.5, 166.5, 10]) cube([3, 4, 40]);
    translate([96.5, 166.5, 10]) cube([3, 4, 40]);
    // alt bolme yan havalandirma (sol)
    translate([-0.5, 11, 18]) cube([4, 3, 26]);
    translate([-0.5, 21, 18]) cube([4, 3, 26]);
    translate([-0.5, 31, 18]) cube([4, 3, 26]);
    // alt bolme yan havalandirma (sag)
    translate([137, 11, 18]) cube([4, 3, 26]);
    translate([137, 21, 18]) cube([4, 3, 26]);
    translate([137, 31, 18]) cube([4, 3, 26]);
    // USB panel gecisi (sol yan) — panel tipi USB uzatmasi icin
    translate([-1, 61, 14.6])
      cube([5, 16, 13]);
    // ana salter (sag yan)
    translate([136, 100, 30])
      rotate([0, 90, 0]) cylinder(h=5, d=12.5, $fn=48);
  }
}

module lid() {
  difference() {
    union() {
      translate([0, 0, BODY_D]) cube([W, H, LID_T]);
      // kapak kenar (ic bosluga oturan dudak)
      translate([WALL + 1.6, WALL + 1.6, BODY_D - 3])
        difference() {
          cube([INT_W - 3.2, INT_H - 3.2, 3.05]);
          translate([1.6, 1.6, -0.5]) cube([INT_W - 6.4, INT_H - 6.4, 4]);
        }
    }
    // OLED penceresi
    translate([W/2 - 28.5, 123, BODY_D - 1])
      cube([57, 30, LID_T + 2]);
    // OLED modulu icin ceplik (panel ic yuzu)
    translate([W/2 - 31, 118, BODY_D - 1])
      cube([62, 40, 1.2]);
    // buton delikleri
    translate([34.5, 107, BODY_D]) cylinder(h=5, d=12.2, $fn=48);
    translate([54.5, 107, BODY_D]) cylinder(h=5, d=12.2, $fn=48);
    translate([74.5, 107, BODY_D]) cylinder(h=5, d=12.2, $fn=48);
    translate([94.5, 107, BODY_D]) cylinder(h=5, d=12.2, $fn=48);
    // RC522 penceresi: tam delik degil, ince et (radiowave gecisi)
    translate([W/2 - 31, 48, BODY_D])
      difference() {
        cube([62, 42, LID_T + 0.02]);
        translate([0, 0, 0])
          cube([62, 42, LID_T - 1.2]);
      }
    // vida delikleri
    translate([11, 11, BODY_D - 0.1]) cylinder(h=5, d=3.4, $fn=24);
    translate([129, 11, BODY_D - 0.1]) cylinder(h=5, d=3.4, $fn=24);
    translate([11, 159, BODY_D - 0.1]) cylinder(h=5, d=3.4, $fn=24);
    translate([129, 159, BODY_D - 0.1]) cylinder(h=5, d=3.4, $fn=24);
  }
}

// Parcalari ayri basmak icin: -D part=body / part=lid
part = "both";
if (part == "body") body();
else if (part == "lid") lid();
else { body(); translate([W + 10, 0, 0]) lid(); }
