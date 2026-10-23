#!/bin/sh
# w4_entry_prep.sh <userdir> - the two scratch libraries of the W4 entry-point walkthrough (docs/w4-integration.md,
# Acceptance item 2). <userdir> is the installation folder's User directory (%APPDATA%\LJ4EVR\LJ4EVR\User on Windows).
# Back up %APPDATA%\LJ4EVR first and delete W4Entry / W4Update afterwards. Everything written here is our own text.
#   W4Entry: Main.Ini [DEBUG] DebugWindow=1 (View > De&bugWindow shown), Fingers.Ini [SETUP] Mode=1 (Virtual Fingers opens
#            without its activation prompt), and fixtures 3-7 already patched and placed: RGB Pack x4 (78), User Definable
#            Generic Scanner 1 (43), Color Scroller 1 (42), Generic Fixture 2 (46), Mac 250 Laser (118), so the icon popup
#            reaches every profile editor and the laser pair. The walkthrough patches fixtures 1-2 itself.
#   W4Update: one format-0 record of each kind the three F56 update dialogs name (cl, bgc, patch) and one other (seq).
set -e
U=${1:?usage: w4_entry_prep.sh <userdir>}
E="$U/W4Entry"
mkdir -p "$E" "$U/W4Aux"   # W4Aux: a spare library row for User Libraries Rename / Backup (sorted after Default, before W4Captions)
printf '[DEBUG]\r\nDebugWindow=1\r\n' > "$E/Main.Ini"
printf '[SETUP]\r\nMode=1\r\n' > "$E/Fingers.Ini"
{
  printf 'LJ4 config 1\r\n'
  printf 'fx 3 kind=builtin id=78 gs2= link=1 addr=33 ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0 user=RGB4 placed=1 x=300 y=200 tab=0 sub=0\r\n'
  printf 'fx 4 kind=builtin id=43 gs2= link=1 addr=49 ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0 user=GSC1 placed=1 x=380 y=200 tab=0 sub=0\r\n'
  printf 'fx 5 kind=builtin id=42 gs2= link=1 addr=65 ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0 user=GCS1 placed=1 x=460 y=200 tab=0 sub=0\r\n'
  printf 'fx 6 kind=builtin id=46 gs2= link=1 addr=81 ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0 user=USER2 placed=1 x=540 y=200 tab=0 sub=0\r\n'
  printf 'fx 7 kind=builtin id=118 gs2= link=1 addr=121 ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0 user=MAC250L placed=1 x=620 y=200 tab=0 sub=0\r\n'
} > "$E/Config.dat"
D="$U/W4Update"
mkdir -p "$D/Cuelists" "$D/BGCues" "$D/Sequences"
printf 'LJ4 cl 0\r\n' > "$D/Cuelists/0099.Cl2"
printf 'LJ4 bgc 0\r\n' > "$D/BGCues/0099.bgc"
printf 'LJ4 patch 0\r\n' > "$D/Old.GPA"
printf 'LJ4 seq 0\r\n' > "$D/Sequences/0099.Seq"
