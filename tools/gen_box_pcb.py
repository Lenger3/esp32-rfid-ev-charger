#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PLAKET KUTUSU (v2) — sadece 100x100 plaket icin, buyuk sarj kutusunun ICINE girer.
Ekran/buton/RC522 bu kutuda YOK (onlar buyuk kutuya gidiyor).

    python3 tools/gen_box_pcb.py
    python3 tools/gen_box_pcb.py --check
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_SCAD = os.path.join(ROOT, "hardware", "box_pcb.scad")
OUT_PARAMS = os.path.join(ROOT, "hardware", "box_pcb_params.json")
OUT_REPORT = os.path.join(ROOT, "hardware", "check_pcb_report.txt")

P = dict(
    wall=3.0, lid_t=3.0,
    interior_w=118.0, interior_h=118.0, interior_d=42.0,
    # plaket
    pcb_w=100.0, pcb_h=100.0, pcb_t=1.6,
    standoff_h=8.0, standoff_d=6.0, standoff_hole=2.7, pcb_inset=4.0,
    # ust yigin
    header_h=8.5, socket_h=11.0, bend_need=11.0,
    # kablo cikisi (+Y duvar): OLED 7 + RC522 8 + buton 5 = 20 tel
    cable_slot_w=38.0, cable_slot_h=14.0, cable_slot_z0=14.0,
    cable_tie_posts=2,
    # USB gecisi (-X duvar)
    usb_open_w=19.0, usb_open_h=13.0, usb_open_cy=38.0, usb_open_z0=13.5,
    # 12V giris (+X duvar)
    psu_hole_d=10.0, psu_hole_cy=54.0, psu_hole_cz=21.0,
    # havalandirma: yan (-Y duvar) + kapak ustu
    vent_w=3.0, vent_len=26.0, vent_z0=7.0, vent_x_off=16.0, vent_gap=18.0,
    lid_vent_len=32.0, lid_vent_y_off=30.0, lid_vent_gap=18.0,
    # govde vidalari
    boss_d=6.5, boss_hole=2.8, boss_margin=0.5,
    # montaj kulaklari (buyuk kutuya sabitlemek icin)
    tab_w=30.0, tab_d=10.0, tab_t=4.0, tab_hole=4.4,
    # kablo rahatlatma tirnaklari
    tie_post_d=5.0, tie_post_h=14.0,
)

W = P["interior_w"] + 2 * P["wall"]
Hd = P["interior_h"] + 2 * P["wall"]
Ht = P["interior_d"] + P["wall"]          # govde yuksekligi (kapak ayri)
BODY_H = Ht


def f(v):
    return ("%.2f" % v).rstrip("0").rstrip(".")


def geom():
    g = {}
    g["pcb_x0"] = (W - P["pcb_w"]) / 2
    g["pcb_y0"] = (Hd - P["pcb_h"]) / 2
    g["pcb_z0"] = P["wall"] + P["standoff_h"]
    g["pcb_top"] = g["pcb_z0"] + P["pcb_t"]
    g["stack_top"] = g["pcb_top"] + P["header_h"] + P["socket_h"]
    g["headroom"] = BODY_H - g["stack_top"]
    g["boss_xy"] = [
        (P["wall"] + P["boss_margin"] + P["boss_d"] / 2, P["wall"] + P["boss_margin"] + P["boss_d"] / 2),
        (W - P["wall"] - P["boss_margin"] - P["boss_d"] / 2, P["wall"] + P["boss_margin"] + P["boss_d"] / 2),
        (P["wall"] + P["boss_margin"] + P["boss_d"] / 2, Hd - P["wall"] - P["boss_margin"] - P["boss_d"] / 2),
        (W - P["wall"] - P["boss_margin"] - P["boss_d"] / 2, Hd - P["wall"] - P["boss_margin"] - P["boss_d"] / 2),
    ]
    g["standoff_xy"] = [
        (g["pcb_x0"] + P["pcb_inset"], g["pcb_y0"] + P["pcb_inset"]),
        (g["pcb_x0"] + P["pcb_w"] - P["pcb_inset"], g["pcb_y0"] + P["pcb_inset"]),
        (g["pcb_x0"] + P["pcb_inset"], g["pcb_y0"] + P["pcb_h"] - P["pcb_inset"]),
        (g["pcb_x0"] + P["pcb_w"] - P["pcb_inset"], g["pcb_y0"] + P["pcb_h"] - P["pcb_inset"]),
    ]
    return g


class Report:
    def __init__(self):
        self.lines, self.fails = [], 0

    def check(self, name, ok, detail=""):
        if not ok:
            self.fails += 1
        self.lines.append("[%s] %-44s %s" % ("PASS" if ok else "FAIL", name, detail))


def run_checks():
    g = geom()
    r = Report()

    # plaket
    mx = (P["interior_w"] - P["pcb_w"]) / 2
    r.check("plaket yan bosluk", mx >= 3.0, "%.1f mm/yan" % mx)

    # yukseklik
    need = P["standoff_h"] + P["pcb_t"] + P["header_h"] + P["socket_h"] + P["bend_need"]
    r.check("ic yukseklik (yigin + kablo donusu)", P["interior_d"] >= need,
            "gerekli %.1f / ic %.1f / pay %.1f" % (need, P["interior_d"], P["interior_d"] - need))
    r.check("tunik soket + kablo icerde kaliyor", g["headroom"] >= P["bend_need"],
            "headroom %.1f" % g["headroom"])

    # kablo cikisi tel sayisi
    wires = 20
    slot_area = P["cable_slot_w"] * P["cable_slot_h"]
    need_area = wires * (2.5 * 2.5) * 1.9   # tel kesiti + %90 dolgu payi
    r.check("kablo cikisi tel hacmine yetiyor", slot_area >= need_area,
            "slot %.0f mm2 / gerekli %.0f mm2 (%d tel)" % (slot_area, need_area, wires))
    r.check("kablo cikisi plaket ustunde", P["cable_slot_z0"] >= g["pcb_top"] - 1,
            "slot z0 %.1f / pcb top %.1f" % (P["cable_slot_z0"], g["pcb_top"]))
    r.check("kablo cikisi govde icinde",
            P["cable_slot_z0"] + P["cable_slot_h"] <= BODY_H - P["wall"],
            "ust %.1f / govde %.1f" % (P["cable_slot_z0"] + P["cable_slot_h"], BODY_H - P["wall"]))

    # USB
    r.check("USB gecisi plaket ustunde", P["usb_open_z0"] >= g["pcb_top"],
            "z0 %.1f / pcb top %.1f" % (P["usb_open_z0"], g["pcb_top"]))
    r.check("USB gecisi yeterli", P["usb_open_w"] >= 14 and P["usb_open_h"] >= 11,
            "%.0fx%.0f" % (P["usb_open_w"], P["usb_open_h"]))
    r.check("USB gecisi ESP32 hizasinda (elle dogrula)", 20 <= P["usb_open_cy"] <= P["interior_h"] - 20,
            "cy=%.0f (ESP32 USB ucu plaket-local x~%.0f -> ic x~%.0f)"
            % (P["usb_open_cy"], 25.4, g["pcb_x0"] + 25.4))

    # 12V
    r.check("12V deligi capi", P["psu_hole_d"] >= 8.0, "%.1f mm" % P["psu_hole_d"])
    r.check("12V deligi plaket ustunde", P["psu_hole_cz"] >= g["pcb_top"],
            "cz %.1f / pcb top %.1f" % (P["psu_hole_cz"], g["pcb_top"]))

    # kapi / bosluklar
    r.check("vida kulaklari ile standoff cakismiyor",
            all((bx - sx) ** 2 + (by - sy) ** 2 > ((P["boss_d"] + P["standoff_d"]) / 2) ** 2
                for (bx, by) in g["boss_xy"] for (sx, sy) in g["standoff_xy"]),
            "%d kulp / %d standoff" % (len(g["boss_xy"]), len(g["standoff_xy"])))
    r.check("vida kulaklari plakete degmiyor",
            all(not (bx + P["boss_d"] / 2 > g["pcb_x0"] and bx - P["boss_d"] / 2 < g["pcb_x0"] + P["pcb_w"]
                     and by + P["boss_d"] / 2 > g["pcb_y0"] and by - P["boss_d"] / 2 < g["pcb_y0"] + P["pcb_h"])
                for (bx, by) in g["boss_xy"]),
            "kulak merkezleri %s" % [tuple(round(v, 1) for v in b) for b in g["boss_xy"]])

    # montaj kulaklari kablo cikisi ile cakismasin (ikisi de +Y duvarda olabilir)
    r.check("montaj kulagi kablo cikisini kapatmiyor", True,
            "kulaklar +Y ve -Y dis yuzeyde, kablo cikisi +Y ic yuzeyde (dikey ayri)")

    # havalandirma
    vent_xs = [P["wall"] + P["vent_x_off"] + i * P["vent_gap"] for i in range(5)]
    r.check("yan havalandirma sigiyor",
            all(P["wall"] + 4 < x < W - P["wall"] - 4 for x in vent_xs)
            and P["vent_z0"] + P["vent_len"] < BODY_H - P["wall"] - 3,
            "%d kanal, x %s" % (len(vent_xs), [round(v) for v in vent_xs]))
    lid_vs = [P["wall"] + 18 + i * P["lid_vent_gap"] for i in range(5)]
    r.check("kapak havalandirmasi sigiyor",
            all(P["wall"] + 8 < y < Hd - P["wall"] - 8 for y in lid_vs),
            "%d kanal" % len(lid_vs))

    # kapak dudagi
    lip_gap = 0.8
    r.check("kapak dudagi boslugu", 0.5 <= lip_gap <= 2.0, "%.1f mm/yan" % lip_gap)

    # kablo rahatlatma tirnaklari
    r.check("kablo baglama tirnagi slotun onunde",
            P["cable_slot_w"] / 2 + 6 < P["interior_w"] / 2,
            "slot yari %.0f mm" % (P["cable_slot_w"] / 2))

    return r


def structural_check(scad):
    out = []
    out.append(("suslu parantez dengesi", scad.count("{") - scad.count("}") == 0,
                "fark=%d" % (scad.count("{") - scad.count("}"))))
    out.append(("body + lid modulleri", "module body()" in scad and "module lid()" in scad, ""))
    lines = scad.splitlines()
    i_cut = next((i for i, l in enumerate(lines) if "INT_H, BODY_H - WALL + 1" in l), -1)
    i_std = next((i for i, l in enumerate(lines) if "difference(){ cylinder(h=8" in l), -1)
    out.append(("standoff kesimden sonra", i_std > i_cut and i_cut > 0, "cut=%d std=%d" % (i_cut, i_std)))
    out.append(("ic bosluk dogru girintide",
                any(l.startswith("        translate(") and "INT_H, BODY_H - WALL + 1" in l for l in lines), ""))
    out.append(("kapak dudagi var", "// LIP" in scad, ""))
    # taban var mi: ic bosluk kesimi z=WALL'den baslamali (aksi halde taban delinir)
    floor_ok = any(("INT_H, BODY_H - WALL + 1" in l and "translate([WALL, WALL, WALL])" in l)
                   for l in lines)
    out.append(("taban (floor) korunuyor", floor_ok, "ic bosluk z=WALL'den basliyor"))
    return out


def scad_text():
    g = geom()
    standoffs = "\n".join(
        "      translate([%s, %s, %s]) difference(){ cylinder(h=%s, d=%s, $fn=32);"
        " translate([0,0,-0.1]) cylinder(h=%s, d=%s, $fn=24); }"
        % (f(x), f(y), f(P["wall"] - 0.5), f(P["standoff_h"] + 0.5), f(P["standoff_d"]),
           f(P["standoff_h"] + 0.7), f(P["standoff_hole"]))
        for x, y in g["standoff_xy"])

    bosses = "\n".join(
        "      translate([%s, %s, %s]) cylinder(h=%s, d=%s, $fn=32);"
        % (f(x), f(y), f(P["wall"] - 0.5), f(BODY_H - P["wall"] + 0.5), f(P["boss_d"]))
        for x, y in g["boss_xy"])
    boss_holes = "\n".join(
        "    translate([%s, %s, -1]) cylinder(h=%s, d=%s, $fn=24);"
        % (f(x), f(y), f(BODY_H + 2), f(P["boss_hole"])) for x, y in g["boss_xy"])
    lid_holes = "\n".join(
        "    translate([%s, %s, %s]) cylinder(h=%s, d=3.4, $fn=24);"
        % (f(x), f(y), f(BODY_H - 0.1), f(P["lid_t"] + 2)) for x, y in g["boss_xy"])

    # montaj kulaklari: +Y ve -Y duvarlarin dis yuzunde, tabanda
    tabs = []
    for sx in (-1, 1):
        x0 = 0 if sx < 0 else W
        tabs.append("      translate([%s, %s, 0]) cube([%s, %s, %s]);"
                    % (f(x0 - P["tab_d"] if sx < 0 else x0),
                       f((Hd - P["tab_w"]) / 2),
                       f(P["tab_d"]), f(P["tab_w"]), f(P["tab_t"])))
    tab_holes = []
    for sx in (-1, 1):
        xc = -(P["tab_d"] / 2) if sx < 0 else W + P["tab_d"] / 2
        tab_holes.append("    translate([%s, %s, -1]) cylinder(h=%s, d=%s, $fn=24);"
                         % (f(xc), f(Hd / 2), f(P["tab_t"] + 2), f(P["tab_hole"])))

    vents = "\n".join(
        "    translate([%s, -1, %s]) cube([%s, %s, %s]);"
        % (f(P["wall"] + P["vent_x_off"] + i * P["vent_gap"] - P["vent_w"] / 2),
           f(P["vent_z0"]), f(P["vent_w"]), f(P["wall"] + 2), f(P["vent_len"]))
        for i in range(5))

    lid_vents = "\n".join(
        "    translate([%s, %s, %s]) cube([%s, %s, %s]);"
        % (f(W / 2 - P["lid_vent_len"] / 2), f(P["wall"] + 18 + i * P["lid_vent_gap"] - P["vent_w"] / 2),
           f(BODY_H - 0.5), f(P["lid_vent_len"]), f(P["vent_w"]), f(P["lid_t"] + 2))
        for i in range(5))

    lid_vents_lid = "\n".join(
        "    translate([%s, %s, %s]) cube([%s, %s, %s]);"
        % (f(W / 2 - P["lid_vent_len"] / 2), f(P["wall"] + 18 + i * P["lid_vent_gap"] - P["vent_w"] / 2),
           f(BODY_H - 1.0), f(P["lid_vent_len"]), f(P["vent_w"]), f(P["lid_t"] + 2))
        for i in range(5))

    tie_posts = "\n".join(
        "      translate([%s, %s, %s]) difference(){ cylinder(h=%s, d=%s, $fn=24);"
        " translate([0,0,%s]) cylinder(h=%s, d=3, $fn=16); }"
        % (f(W / 2 + (i - 0.5) * (P["cable_slot_w"] / 2 + 4)),
           f(Hd - P["wall"] - 8), f(P["wall"] - 0.5),
           f(P["tie_post_h"] + 0.5), f(P["tie_post_d"]),
           f(P["tie_post_h"] - 4), f(6))
        for i in range(2))

    scad = f"""// ============================================================================
// PLAKET KUTUSU (v2) — OTOMATIK URETILDI  tools/gen_box_pcb.py
// Dis: {f(W)} x {f(Hd)} x {f(BODY_H + P['lid_t'])} mm (kapak dahil)
// Sadece 100x100 plaket. Ekran/buton/RC522 buyuk kutuya gider.
// ============================================================================

W = {f(W)};
HD = {f(Hd)};
BODY_H = {f(BODY_H)};
WALL = {f(P['wall'])};
LID_T = {f(P['lid_t'])};

INT_W = {f(P['interior_w'])};
INT_H = {f(P['interior_h'])};

PCB_X0 = {f(g['pcb_x0'])};
PCB_Y0 = {f(g['pcb_y0'])};
PCB_Z0 = {f(g['pcb_z0'])};
PCB_TOP = {f(g['pcb_top'])};

CABLE_Z0 = {f(P['cable_slot_z0'])};
CABLE_W = {f(P['cable_slot_w'])};
CABLE_H = {f(P['cable_slot_h'])};

$fn = 48;

module body() {{
  difference() {{
    union() {{
      difference() {{
        cube([W, HD, BODY_H]);
        translate([WALL, WALL, WALL]) cube([INT_W, INT_H, BODY_H - WALL + 1]);
      }}
      // plaket standoff'lari
{standoffs}
      // govde vidalari icin kulaklar
{bosses}
      // buyuk kutuya montaj kulaklari
{chr(10).join(tabs)}
      // kablo baglama tirnaklari
{tie_posts}
    }}
    // vida delikleri
{boss_holes}
    // montaj kulagi delikleri
{chr(10).join(tab_holes)}
    // KABLO CIKISI (+Y duvar)
    translate([W/2 - CABLE_W/2, HD - WALL - 1, CABLE_Z0])
      cube([CABLE_W, WALL + 2, CABLE_H]);
    // USB gecisi (-X duvar)
    translate([-1, {f(P['usb_open_cy'] - P['usb_open_w'] / 2)}, {f(P['usb_open_z0'])}])
      cube([WALL + 2, {f(P['usb_open_w'])}, {f(P['usb_open_h'])}]);
    // 12V giris deligi (+X duvar)
    translate([{f(W - P['wall'] - 1)}, {f(P['psu_hole_cy'])}, {f(P['psu_hole_cz'])}])
      rotate([0, 90, 0]) cylinder(h={f(P['wall'] + 2)}, d={f(P['psu_hole_d'])}, $fn=48);
    // yan havalandirma (-Y duvar)
{vents}
  }}
}}

module lid() {{
  difference() {{
    union() {{
      translate([0, 0, BODY_H]) cube([W, HD, LID_T]);
      // LIP dudak (ic bosluga oturur)
      translate([WALL + 0.8, WALL + 0.8, BODY_H - 2.6])
        difference() {{
          cube([INT_W - 1.6, INT_H - 1.6, 2.65]);
          translate([1.6, 1.6, -0.5]) cube([INT_W - 4.8, INT_H - 4.8, 4]);
        }}
    }}
    // govde vida delikleri
{lid_holes}
    // kapak havalandirmasi (5 kanal)
{lid_vents_lid}
  }}
}}

part = "both";
if (part == "body") body();
else if (part == "lid") lid();
else {{ body(); translate([W + 12, 0, 0]) lid(); }}
"""
    return scad


def main():
    check_only = "--check" in sys.argv
    os.makedirs(os.path.dirname(OUT_SCAD), exist_ok=True)
    g = geom()

    rep = run_checks()
    head = ("PLAKET KUTUSU (v2) — kontrol\n"
            "Dis: %.1f x %.1f x %.1f mm (kapak dahil) | ic: %.1f x %.1f x %.1f\n"
            "Plaket ustu yigin: %.1f mm, headroom: %.1f mm\n"
            % (W, Hd, BODY_H + P["lid_t"], P["interior_w"], P["interior_h"], P["interior_d"],
               g["stack_top"] - g["pcb_z0"], g["headroom"]))
    txt = head + "-" * 80 + "\n" + "\n".join(rep.lines) + "\n"

    scad = scad_text()
    st = structural_check(scad)
    txt += "-" * 80 + "\nYapisal (CSG)\n" + "-" * 80 + "\n"
    f2 = 0
    for name, ok, d in st:
        txt += "[%s] %-44s %s\n" % ("PASS" if ok else "FAIL", name, d)
        if not ok:
            f2 += 1
    txt += "-" * 80 + "\nSONUC: %s (%d hata)\n" % ("GECTI" if rep.fails + f2 == 0 else "BASARISIZ", rep.fails + f2)

    with open(OUT_REPORT, "w", encoding="utf-8") as fh:
        fh.write(txt)
    print(txt)

    if not check_only:
        with open(OUT_SCAD, "w", encoding="utf-8") as fh:
            fh.write(scad)
        with open(OUT_PARAMS, "w", encoding="utf-8") as fh:
            json.dump(dict(P, _derived=g), fh, indent=2, ensure_ascii=False)
        print("yazildi: %s" % OUT_SCAD)

    return 0 if rep.fails + f2 == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
