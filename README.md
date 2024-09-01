# scrollreading
Code and experiments related to reading the Herculanium Papyri

report.pdf - a report describing this work

*.png - some of the images that appear in report.pdf

scrollprocess.py - simple user interface for viewing x,y-plane slices, quickly moving up and down in the z-plane, and invoking processing operations

scrollprocess.c - contains processing functions used by scrollprocess.py. Compiles to a DLL, loaded by scrollprocess.py

holefiller.c - implementation of DAFF described in report.pdf

neighbours.py - stitches together neighbouring-volume surfaces

neighbours_in_vol.py - stitches together in-volume surface patches

fill3d.cpp - Experimental Hashlife 3D fill. Not used by anything else in this project.
