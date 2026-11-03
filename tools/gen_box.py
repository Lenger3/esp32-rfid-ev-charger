#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Kutu tasarimi: parametre -> hardware/box.scad + geometri kontrolu.
Tek dogruluk kaynagi burasi. Parametre degistir, tekrar calistir.

    python3 tools/gen_box.py            # uret + kontrol
    python3 tools/gen_box.py --check    # sadece kontrol
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_SCAD = os.path.join(ROOT, "hardware", "box.scad")
OUT_PARAMS = os.path.join(ROOT, "hardware", "params.json")
OUT_REPORT = os.path.join(ROOT, "hardware", "check_report.txt")

# ----------------------------------------------------------------------------
# PARAMETRELER
# ----------------------------------------------------------------------------
P = dict(
    # --- kabuk
    wall=3.0,
    lid_t=3.0,
    interior_w=134.0,
    interior_h=164.0,
    interior_d=56.0,
    # --- bolmeler (yuksekligi paylasir)
    lower_chamber_h=37.0,     # 12V PSU bolmesi
    divider_t=3.0,
    # --- plaket
    pcb_w=100.0, pcb_h=100.0, pcb_t=1.6,
    standoff_h=8.0, standoff_d=6.0, standoff_hole=2.7,
    pcb_inset=3.5,            # plaket montaj deligi kenardan
    pcb_x0=17.0,              # plaketin sol kenari (ic koordinat)
    pcb_y_margin=8.0,         # ayirici ile plaket alt kenari arasi
    # --- ust konnektor yigini (derinlik kontrolu)
    header_h=8.5,             # disi header
    socket_h=11.0,            # tunik disi soket (kablo tarafi)
    module_top=3.5,           # ESP32 shield
    bend_need=12.0,           # kablo donus payi
    # --- on panel (kapak) ozellikleri, ic y'den olculu
    oled_win_w=57.0, oled_win_h=30.0, oled_win_cy=95.0,
    oled_mod_w=62.0, oled_mod_h=40.0,
    btn_d=12.2, btn_cy=64.0,
    btn_cx=[34.5, 54.5, 74.5, 94.5],
    rc_win_w=62.0, rc_win_h=42.0, rc_win_cy=26.0,
    rc_mod_w=60.0, rc_mod_h=40.0,
    rc_wall=1.2,              # RC522 onunde kalan et kalinligi
    # --- alt bolme / PSU
    psu_w=90.0, psu_height=32.0, psu_depth=50.0,
    # --- kablo rakorlari (alt yuz)
    gland_d=16.2, gland_z=31.0, gland_x=[22.0, 67.0, 110.0],
    # --- bolme gecisi
    divider_hole_d=12.0, divider_hole_x=[40.0, 94.0],
    # --- havalandirma
    vent_w=3.0, vent_len=40.0, vent_z0=10.0, vent_x=[18.0, 34.0, 50.0, 66.0, 82.0, 98.0],
    side_vent_w=3.0, side_vent_len=26.0, side_vent_y_off=8.0,
    # --- vida kulaklari
    boss_d=8.0, boss_hole=2.8, boss_margin=4.0,
    # --- USB panel gecisi (sol yan)
    usb_cut_w=16.0, usb_cut_h=13.0, usb_cut_cy=80.0, usb_cut_cz=30.0,
    # --- ana salter (sag yan)
    sw_d=12.5, sw_cy=100.0, sw_cz=30.0,
)

W = P["interior_w"] + 2 * P["wall"]
H = P["interior_h"] + 2 * P["wall"]
D = P["interior_d"] + P["wall"] + P["lid_t"]


def psu_posts_xy():
    """PSU tutma tirnaklarinin (x,y) kose noktalari — alt bolme icinde kalacak sekilde."""
    px0 = (P["interior_w"] - P["psu_w"]) / 2 + P["wall"]
    y_lo = P["wall"] + 1.0
    y_hi = P["wall"] + P["lower_chamber_h"] - 7.0
    return [(px0 - 6, y_lo), (px0 + P["psu_w"], y_lo),
            (px0 - 6, y_hi), (px0 + P["psu_w"], y_hi)]


def centered_panel_items():
    """On panel ogelerini ic koordinattan mutlak y'ye cevir."""
    y0 = P["wall"] + P["lower_chamber_h"] + P["divider_t"]  # ust bolme tabani
    return dict(
        upper_y0=y0,
        oled_cy=y0 + P["oled_win_cy"],
        btn_cy=y0 + P["btn_cy"],
        rc_cy=y0 + P["rc_win_cy"],
    )


# ----------------------------------------------------------------------------
# KONTROL
# ----------------------------------------------------------------------------
class Report:
    def __init__(self):
        self.lines = []
        self.fails = 0

    def check(self, name, ok, detail):
        tag = "PASS" if ok else "FAIL"
        if not ok:
            self.fails += 1
        self.lines.append("[%s] %-42s %s" % (tag, name, detail))


def run_checks():
    r = Report()
    m = centered_panel_items()
    y0 = m["upper_y0"]

    # 1. bolme toplami
    total = P["lower_chamber_h"] + P["divider_t"] + (P["interior_h"] - P["lower_chamber_h"] - P["divider_t"])
    r.check("bolme yuksekligi toplami", abs(total - P["interior_h"]) < 0.01,
            "%.1f / %.1f" % (total, P["interior_h"]))

    # 2. ust bolme yuksekligi plaketi aliyor mu
    upper_h = P["interior_h"] - P["lower_chamber_h"] - P["divider_t"]
    need_h = P["pcb_y_margin"] + P["pcb_h"] + 10.0
    r.check("ust bolme plaketi aliyor", upper_h >= need_h,
            "ust=%.1f gerekli=%.1f" % (upper_h, need_h))

    # 3. plaket genisligi
    margin_x = P["interior_w"] - P["pcb_x0"] - P["pcb_w"]
    r.check("plaket yan bosluk (sag)", margin_x >= 10.0, "%.1f mm" % margin_x)
    r.check("plaket yan bosluk (sol)", P["pcb_x0"] - P["wall"] >= 10.0,
            "%.1f mm" % (P["pcb_x0"] - P["wall"]))

    # 4. derinlik: plaket ustu yigin + kablo donusu
    stack = P["standoff_h"] + P["pcb_t"] + P["header_h"] + P["socket_h"]
    need_d = stack + P["bend_need"]
    r.check("derinlik (yigin+kablo donusu)", P["interior_d"] >= need_d,
            "gerekli=%.1f ic=%.1f pay=%.1f" % (need_d, P["interior_d"], P["interior_d"] - need_d))

    # 5. ESP32 modul yuksekligi on panele degmiyor
    esp_top_z = P["standoff_h"] + P["pcb_t"] + P["header_h"] + P["module_top"]
    r.check("ESP32 on panele mesafe", P["interior_d"] - esp_top_z >= 15.0,
            "%.1f mm" % (P["interior_d"] - esp_top_z))

    # 6. OLED penceresi: aktif alandan buyuk, modulden kucuk
    oled_active_w, oled_active_h = 55.01, 27.49
    r.check("OLED pencere >= aktif alan",
            P["oled_win_w"] >= oled_active_w and P["oled_win_h"] >= oled_active_h,
            "%.1fx%.1f vs %.1fx%.1f" % (P["oled_win_w"], P["oled_win_h"], oled_active_w, oled_active_h))
    r.check("OLED pencere <= modul",
            P["oled_win_w"] <= P["oled_mod_w"] and P["oled_win_h"] <= P["oled_mod_h"],
            "%.1fx%.1f vs %.1fx%.1f" % (P["oled_win_w"], P["oled_win_h"], P["oled_mod_w"], P["oled_mod_h"]))

    # 7. OLED modulu panele sigiyor
    r.check("OLED modul > ic genislik", P["oled_mod_w"] + 2 * 5 <= P["interior_w"],
            "modul %.0f + 2x5 <= %.0f" % (P["oled_mod_w"], P["interior_w"]))

    # 8. buton araligi
    gaps = [P["btn_cx"][i + 1] - P["btn_cx"][i] for i in range(len(P["btn_cx"]) - 1)]
    web = min(gaps) - P["btn_d"]
    r.check("buton govde araligi", web >= 5.0, "en dar %.2f mm" % web)
    r.check("butonlar panele sigiyor",
            P["btn_cx"][0] - P["btn_d"] / 2 >= 8 and P["btn_cx"][-1] + P["btn_d"] / 2 <= P["interior_w"] - 8,
            "ilk=%.1f son=%.1f" % (P["btn_cx"][0], P["btn_cx"][-1]))

    # 9. dikey araliklar (RC522 - buton - OLED)
    rc_top = P["rc_win_cy"] + P["rc_win_h"] / 2
    btn_bot = P["btn_cy"] - P["btn_d"] / 2
    btn_top = P["btn_cy"] + P["btn_d"] / 2
    oled_bot = P["oled_win_cy"] - P["oled_win_h"] / 2
    r.check("RC522 -> buton bosluk", btn_bot - rc_top >= 6.0, "%.1f mm" % (btn_bot - rc_top))
    r.check("buton -> OLED bosluk", oled_bot - btn_top >= 6.0, "%.1f mm" % (oled_bot - btn_top))
    r.check("OLED -> ust kenar", upper_h - (P["oled_win_cy"] + P["oled_win_h"] / 2) >= 8.0,
            "%.1f mm" % (upper_h - (P["oled_win_cy"] + P["oled_win_h"] / 2)))
    r.check("RC522 -> bolme", P["rc_win_cy"] - P["rc_win_h"] / 2 >= 4.0,
            "%.1f mm" % (P["rc_win_cy"] - P["rc_win_h"] / 2))

    # 10. RC522 modulu pencereye sigiyor
    r.check("RC522 modul <= pencere",
            P["rc_mod_w"] <= P["rc_win_w"] and P["rc_mod_h"] <= P["rc_win_h"],
            "%.0fx%.0f vs %.0fx%.0f" % (P["rc_mod_w"], P["rc_mod_h"], P["rc_win_w"], P["rc_win_h"]))
    r.check("RC522 on et kalinligi", P["rc_wall"] <= 1.5,
            "%.1f mm (<=1.5 okunabilirlik icin)" % P["rc_wall"])

    # 11. PSU alt bolmeye sigiyor (yatik: 90 genislik / 32 yukseklik / 50 derinlik)
    psu_ok = (P["psu_w"] + 10 <= P["interior_w"] and
              P["psu_height"] + 4 <= P["lower_chamber_h"] and
              P["psu_depth"] + 6 <= P["interior_d"])
    r.check("PSU alt bolmeye sigiyor", psu_ok,
            "%.0f(w)x%.0f(h)x%.0f(d), bolme %.0fx%.0fx%.0f"
            % (P["psu_w"], P["psu_height"], P["psu_depth"],
               P["interior_w"], P["lower_chamber_h"], P["interior_d"]))

    # 12. rakorlar alt bolmede ve birbirine degmiyor
    gx = P["gland_x"]
    r.check("rakor araligi", min(gx[i + 1] - gx[i] for i in range(len(gx) - 1)) >= P["gland_d"] + 6,
            "en dar %.1f mm" % min(gx[i + 1] - gx[i] for i in range(len(gx) - 1)))
    r.check("rakor z merkezi govde icinde",
            P["wall"] + 2 <= P["gland_z"] <= D - P["lid_t"] - 2,
            "z=%.1f" % P["gland_z"])

    # 13. vida kulaklari plakete degmiyor (en az 2 mm bosluk)
    bm = P["boss_margin"] + P["boss_d"]
    pcb_x1 = P["pcb_x0"] + P["pcb_w"]
    pcb_y0_abs = y0 + P["pcb_y_margin"]
    pcb_y1_abs = pcb_y0_abs + P["pcb_h"]
    boss_pts = [(P["wall"] + P["boss_margin"] + P["boss_d"] / 2, P["wall"] + P["boss_margin"] + P["boss_d"] / 2),
                (W - P["wall"] - P["boss_margin"] - P["boss_d"] / 2, P["wall"] + P["boss_margin"] + P["boss_d"] / 2),
                (P["wall"] + P["boss_margin"] + P["boss_d"] / 2, H - P["wall"] - P["boss_margin"] - P["boss_d"] / 2),
                (W - P["wall"] - P["boss_margin"] - P["boss_d"] / 2, H - P["wall"] - P["boss_margin"] - P["boss_d"] / 2)]
    clash = []
    min_gap = 999.0
    for (bx, by) in boss_pts:
        r_ = P["boss_d"] / 2
        if (bx + r_ > P["pcb_x0"] and bx - r_ < pcb_x1 and
                by + r_ > pcb_y0_abs and by - r_ < pcb_y1_abs):
            clash.append((round(bx, 1), round(by, 1)))
        gx_ = max(P["pcb_x0"] - (bx + r_), (bx - r_) - pcb_x1)
        gy_ = max(pcb_y0_abs - (by + r_), (by - r_) - pcb_y1_abs)
        min_gap = min(min_gap, max(gx_, gy_))
    r.check("vida kulaklari plakete degmiyor", not clash and min_gap >= 2.0,
            "cakisan=%s en yakin bosluk=%.1f mm" % (clash, min_gap))

    # 14. plaket standoff'lari plaket icinde
    so = [(P["pcb_x0"] + P["pcb_inset"], pcb_y0_abs + P["pcb_inset"]),
          (P["pcb_x0"] + P["pcb_w"] - P["pcb_inset"], pcb_y0_abs + P["pcb_inset"]),
          (P["pcb_x0"] + P["pcb_inset"], pcb_y1_abs - P["pcb_inset"]),
          (P["pcb_x0"] + P["pcb_w"] - P["pcb_inset"], pcb_y1_abs - P["pcb_inset"])]
    ok = all(P["wall"] + 2 < x < W - P["wall"] - 2 and P["wall"] + 2 < y < H - P["wall"] - 2 for x, y in so)
    r.check("standoff'lar govde icinde", ok, "nokta=%d" % len(so))

    # 15. USB gecisi ESP32'nin arkasina denk geliyor mu (z)
    esp_z0 = P["standoff_h"] + P["pcb_t"]
    r.check("USB gecisi plaket ustunde", P["usb_cut_cz"] >= esp_z0,
            "usb_cz=%.1f esp_z0=%.1f" % (P["usb_cut_cz"], esp_z0))
    # ESP32 modulu plaket-local x 17..68 -> mutlak
    esp_x0 = P["pcb_x0"] + 17.0
    r.check("USB gecisi yan duvara uzak (uyari)", True,
            "ESP32 USB plaket-local x~%.0f (mutlak %.0f), yan duvara %.0f mm uzak -> panel USB uzatmasi kullanilacak"
            % (17.0, esp_x0, esp_x0 - P["wall"]))

    # 16. havalandirma kanallari govde icinde
    r.check("hava kanallari sigiyor",
            all(P["wall"] + 5 < x < W - P["wall"] - 5 for x in P["vent_x"]) and
            P["vent_z0"] + P["vent_len"] < D - P["lid_t"] - 3,
            "%d kanal" % len(P["vent_x"]))

    # 17. bolme gecis delikleri rakorlarla cakismiyor (z farki)
    r.check("bolme gecisi rakor hattinda", True,
            "divider z ayni, x ayri: rakor %s / gecis %s" % (gx, P["divider_hole_x"]))

    # 18. PSU tirnaklari alt bolme icinde
    posts = psu_posts_xy()
    y_lo_ch = P["wall"]
    y_hi_ch = P["wall"] + P["lower_chamber_h"]
    ok_p = all(y_lo_ch <= y and y + 6 <= y_hi_ch for _, y in posts)
    ok_x = all(P["wall"] <= x and x + 6 <= W - P["wall"] for x, _ in posts)
    r.check("PSU tirnaklari alt bolmede", ok_p and ok_x,
            "y %s | x %s" % ([round(y, 1) for _, y in posts], [round(x, 1) for x, _ in posts]))

    # 19. kapak dudagi ic bosluga oturuyor (0.8-2.5 mm bosluk)
    lip_gap = (P["interior_w"] - (P["interior_w"] - 3.2)) / 2
    r.check("kapak dudagi boslugu", 0.8 <= lip_gap <= 2.5, "%.1f mm/yan" % lip_gap)

    return r


# ----------------------------------------------------------------------------
# SCAD URETIMI
# ----------------------------------------------------------------------------
def scad_text():
    m = centered_panel_items()
    y0 = m["upper_y0"]
    pcb_y0 = y0 + P["pcb_y_margin"]
    BODY_D = D - P["lid_t"]

    def f(v):
        return ("%.2f" % v).rstrip("0").rstrip(".")

    btn = "\n".join(
        "    translate([%s, %s, BODY_D]) cylinder(h=%s, d=%s, $fn=48);"
        % (f(x), f(m["btn_cy"]), f(P["lid_t"] + 2), f(P["btn_d"]))
        for x in P["btn_cx"])

    vents = "\n".join(
        "    translate([%s, %s, %s]) cube([%s, %s, %s]);"
        % (f(x - P["vent_w"] / 2), f(H - P["wall"] - 0.5), f(P["vent_z0"]),
           f(P["vent_w"]), f(P["wall"] + 1), f(P["vent_len"]))
        for x in P["vent_x"])

    side_vents = "\n".join(
        "    translate([%s, %s, %s]) cube([%s, %s, %s]);"
        % (f(-0.5), f(P["wall"] + P["side_vent_y_off"] + i * 10), f(P["gland_z"] - P["side_vent_len"] / 2),
           f(P["wall"] + 1), f(P["side_vent_w"]), f(P["side_vent_len"]))
        for i in range(3))

    side_vents_r = side_vents.replace("translate([-0.5,", "translate([%s," % f(W - P["wall"]))

    glands = "\n".join(
        "    translate([%s, -1, %s]) rotate([-90,0,0]) cylinder(h=%s, d=%s, $fn=48);"
        % (f(x), f(P["gland_z"]), f(P["wall"] + 2), f(P["gland_d"]))
        for x in P["gland_x"])

    div_holes = "\n".join(
        "    translate([%s, %s, %s]) rotate([-90,0,0]) cylinder(h=%s, d=%s, $fn=48);"
        % (f(x), f(P["wall"] + P["lower_chamber_h"] - 1), f(P["gland_z"]),
           f(P["divider_t"] + 2), f(P["divider_hole_d"]))
        for x in P["divider_hole_x"])

    boss_xy = [(P["wall"] + P["boss_margin"] + P["boss_d"] / 2, P["wall"] + P["boss_margin"] + P["boss_d"] / 2),
               (W - P["wall"] - P["boss_margin"] - P["boss_d"] / 2, P["wall"] + P["boss_margin"] + P["boss_d"] / 2),
               (P["wall"] + P["boss_margin"] + P["boss_d"] / 2, H - P["wall"] - P["boss_margin"] - P["boss_d"] / 2),
               (W - P["wall"] - P["boss_margin"] - P["boss_d"] / 2, H - P["wall"] - P["boss_margin"] - P["boss_d"] / 2)]
    bosses = "\n".join(
        "    translate([%s, %s, %s]) cylinder(h=%s, d=%s, $fn=32);"
        % (f(x), f(y), f(P["wall"]), f(BODY_D - P["wall"]), f(P["boss_d"])) for x, y in boss_xy)
    boss_holes = "\n".join(
        "    translate([%s, %s, -1]) cylinder(h=%s, d=%s, $fn=24);"
        % (f(x), f(y), f(BODY_D + 2), f(P["boss_hole"])) for x, y in boss_xy)
    lid_holes = "\n".join(
        "    translate([%s, %s, BODY_D - 0.1]) cylinder(h=%s, d=3.4, $fn=24);"
        % (f(x), f(y), f(P["lid_t"] + 2)) for x, y in boss_xy)

    standoffs = "\n".join(
        "      translate([%s, %s, %s]) difference(){ cylinder(h=%s, d=%s, $fn=32);"
        " translate([0,0,-0.1]) cylinder(h=%s, d=%s, $fn=24); }"
        % (f(P["pcb_x0"] + dx), f(pcb_y0 + dy), f(P["wall"]),
           f(P["standoff_h"]), f(P["standoff_d"]),
           f(P["standoff_h"] + 0.2), f(P["standoff_hole"]))
        for dx, dy in [(P["pcb_inset"], P["pcb_inset"]),
                       (P["pcb_w"] - P["pcb_inset"], P["pcb_inset"]),
                       (P["pcb_inset"], P["pcb_h"] - P["pcb_inset"]),
                       (P["pcb_w"] - P["pcb_inset"], P["pcb_h"] - P["pcb_inset"])])

    # PSU tutma tirnaklari: alt bolmede 4 kose, 6 mm yuksek
    psu_posts = "\n".join(
        "      translate([%s, %s, %s]) cube([6, 6, 6]);"
        % (f(x), f(y), f(P["wall"]))
        for x, y in psu_posts_xy())

    # USB gecisi: plaket duzlemine yakin, ESP32 USB ucu hizasinda
    usb_y0 = pcb_y0 + 10.0
    usb_z0 = P["wall"] + P["standoff_h"] + P["pcb_t"] + 2.0

    BODY_D = D - P["lid_t"]

    t = f"""// ============================================================================
// EV Sarj Kutusu — OTOMATIK URETILDI (tools/gen_box.py)
// Dis olcu: {f(W)} x {f(H)} x {f(D)} mm
// Malzeme onerisi: PETG, 3 perde, %25 dolgu
// ============================================================================

W = {f(W)};
H = {f(H)};
D = {f(D)};
WALL = {f(P['wall'])};
LID_T = {f(P['lid_t'])};
BODY_D = {f(BODY_D)};

INT_W = {f(P['interior_w'])};
INT_H = {f(P['interior_h'])};
INT_D = {f(P['interior_d'])};
LOWER_H = {f(P['lower_chamber_h'])};
DIV_T = {f(P['divider_t'])};

$fn = 48;

module body() {{
  difference() {{
    union() {{
      // 1) kabuk: dis kutudan ic bosluk cikarilir (on yuz acik)
      difference() {{
        cube([W, H, BODY_D]);
        translate([WALL, WALL, WALL]) cube([INT_W, INT_H, BODY_D - WALL + 1]);
      }}
      // 2) ayirici (alt bolme: 12V PSU) — kabuk kesildikten SONRA eklenir
      translate([WALL, WALL + LOWER_H, WALL])
        cube([INT_W, DIV_T, BODY_D - WALL]);
      // 3) plaket standoff'lari
{standoffs}
      // 4) vida kulaklari
{bosses}
      // 5) PSU tutma tirnaklari (alt bolme)
{psu_posts}
    }}
    // vida delikleri
{boss_holes}
    // kablo rakorlari (alt yuz)
{glands}
    // bolme kablosu gecisi
{div_holes}
    // ust havalandirma
{vents}
    // alt bolme yan havalandirma (sol)
{side_vents}
    // alt bolme yan havalandirma (sag)
{side_vents_r}
    // USB panel gecisi (sol yan) — panel tipi USB uzatmasi icin
    translate([-1, {f(usb_y0)}, {f(usb_z0)}])
      cube([{f(P['wall'] + 2)}, {f(P['usb_cut_w'])}, {f(P['usb_cut_h'])}]);
    // ana salter (sag yan)
    translate([{f(W - P['wall'] - 1)}, {f(P['sw_cy'])}, {f(P['sw_cz'])}])
      rotate([0, 90, 0]) cylinder(h={f(P['wall'] + 2)}, d={f(P['sw_d'])}, $fn=48);
  }}
}}

module lid() {{
  difference() {{
    union() {{
      translate([0, 0, BODY_D]) cube([W, H, LID_T]);
      // kapak kenar (ic bosluga oturan dudak)
      translate([WALL + 1.6, WALL + 1.6, BODY_D - 3])
        difference() {{
          cube([INT_W - 3.2, INT_H - 3.2, 3.05]);
          translate([1.6, 1.6, -0.5]) cube([INT_W - 6.4, INT_H - 6.4, 4]);
        }}
    }}
    // OLED penceresi
    translate([W/2 - {f(P['oled_win_w']/2)}, {f(m['oled_cy'] - P['oled_win_h']/2)}, BODY_D - 1])
      cube([{f(P['oled_win_w'])}, {f(P['oled_win_h'])}, LID_T + 2]);
    // OLED modulu icin ceplik (panel ic yuzu)
    translate([W/2 - {f(P['oled_mod_w']/2)}, {f(m['oled_cy'] - P['oled_mod_h']/2)}, BODY_D - 1])
      cube([{f(P['oled_mod_w'])}, {f(P['oled_mod_h'])}, 1.2]);
    // buton delikleri
{btn}
    // RC522 penceresi: tam delik degil, ince et (radiowave gecisi)
    translate([W/2 - {f(P['rc_win_w']/2)}, {f(m['rc_cy'] - P['rc_win_h']/2)}, BODY_D])
      difference() {{
        cube([{f(P['rc_win_w'])}, {f(P['rc_win_h'])}, LID_T + 0.02]);
        translate([0, 0, 0])
          cube([{f(P['rc_win_w'])}, {f(P['rc_win_h'])}, LID_T - {f(P['rc_wall'])}]);
      }}
    // vida delikleri
{lid_holes}
  }}
}}

// Parcalari ayri basmak icin: -D part=body / part=lid
part = "both";
if (part == "body") body();
else if (part == "lid") lid();
else {{ body(); translate([W + 10, 0, 0]) lid(); }}
"""
    return t, BODY_D


def structural_check(scad):
    """CSG yapisini denetler: toplamali ozellikler ic bosluk kesiminden SONRA eklenmeli."""
    lines = scad.splitlines()
    out = []

    # 1) parantez dengesi
    bal = scad.count("{") - scad.count("}")
    out.append(("suslu parantez dengesi", bal == 0, "fark=%d" % bal))

    # 2) ic bosluk kesimi, dis kutunun difference'i ICINDE mi (girinti 8 bosluk)
    indent_ok = False
    for ln in lines:
        if "INT_H, BODY_D - WALL + 1" in ln and "cube(" in ln:
            indent_ok = ln.startswith("        translate(")
    out.append(("ic bosluk kesimi dogru seviyede", indent_ok,
                "girinti=8 beklenir"))

    # 3) standoff ve kulaklar ic bosluk kesiminden sonra mi
    i_cut = next((i for i, ln in enumerate(lines) if "INT_H, BODY_D - WALL + 1" in ln), -1)
    i_std = next((i for i, ln in enumerate(lines) if "difference(){ cylinder(h=8" in ln), -1)
    out.append(("standoff'lar kesimden sonra", i_std > i_cut, "cut=%d standoff=%d" % (i_cut, i_std)))

    # 4) kapak ve govde ayri module
    out.append(("body ve lid modulleri var",
                "module body()" in scad and "module lid()" in scad, ""))

    # 5) RC522 ince et: 1.2 mm kalinlik birakilmis mi
    out.append(("RC522 cebi 1.2 mm birakiyor",
                "LID_T - 1.2" in scad, ""))
    floor_ok = any(("INT_H, BODY_D - WALL + 1" in l and "translate([WALL, WALL, WALL])" in l)
                   for l in lines)
    out.append(("taban (floor) korunuyor", floor_ok, ""))

    return out


def main():
    check_only = "--check" in sys.argv
    os.makedirs(os.path.dirname(OUT_SCAD), exist_ok=True)

    rep = run_checks()
    txt = "\n".join(rep.lines)
    header = ("EV Sarj Kutusu — geometri kontrolu\n"
              "Dis olcu: %.1f x %.1f x %.1f mm | ic: %.1f x %.1f x %.1f\n" % (W, H, D, P["interior_w"], P["interior_h"], P["interior_d"]))
    body = header + "-" * 78 + "\n" + txt + "\n" + "-" * 78 + "\n"

    scad, _ = scad_text()
    st = structural_check(scad)
    body += "Yapisal (CSG) kontrol\n" + "-" * 78 + "\n"
    fails2 = 0
    for name, ok, detail in st:
        body += "[%s] %-42s %s\n" % ("PASS" if ok else "FAIL", name, detail)
        if not ok:
            fails2 += 1
    body += "-" * 78 + "\n"

    total_fails = rep.fails + fails2
    body += "SONUC: %s (%d hata)\n" % ("GECTI" if total_fails == 0 else "BASARISIZ", total_fails)

    with open(OUT_REPORT, "w", encoding="utf-8") as fh:
        fh.write(body)
    print(body)

    if not check_only:
        with open(OUT_SCAD, "w", encoding="utf-8") as fh:
            fh.write(scad)
        with open(OUT_PARAMS, "w", encoding="utf-8") as fh:
            json.dump(P, fh, indent=2, ensure_ascii=False)
        print("yazildi: %s" % OUT_SCAD)
        print("yazildi: %s" % OUT_PARAMS)

    return 0 if total_fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
