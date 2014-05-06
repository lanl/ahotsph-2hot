#!/usr/bin/env python
from yt.mods import *
from yt.analysis_modules.halo_finding.api import *

files = ["/lustre/atlas1/ast102/proj-shared/yt-project/data/Enzo_64/DD0043/data0043"]

pf = load(files[0])

halo_list = HaloFinder(pf)
for halo in halo_list[:5]:
    print halo.total_mass()

fof_halo_list = FOFHaloFinder(pf)
for halo in fof_halo_list[:5]:
    print halo.total_mass()


pf = load(files[0])

halo_list = HaloFinder(pf)
for halo in halo_list[:5]:
    print halo.total_mass()

fof_halo_list = FOFHaloFinder(pf)
for halo in fof_halo_list[:5]:
    print halo.total_mass()

