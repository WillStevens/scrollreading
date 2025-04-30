# scrollreading
Code and experiments related to reading the Herculanium Papyri.

The aim of this project is to put together a fast autosegmentation and ink-detection pipeline. Work is currently focussed on the autosegmantation stages.

Autosegmentation is performed one cubic 512x512x512 volume at a time by using algorithms derived from flood-fill. One algorithm (called FAFF - flatness affinity flood-fill) tries to fill only flattish areas, in the hope that busy areas where surfaces meet each other are not filled. Another algorithm (called DAFF - damage avoiding flood-fill) tries to detect and plug areas that cause intersection. Both algorithms are currently being evaluated and investigated to understand their behaviour and to optimize them.

Once surface patches have been identified, they are rendered immediately using a simple projection to a 2D plane for visual inspection, and are stitched together within each cubic volume. Once cubic volume surfaces patches have been stitched together, surfaces from neighbouring volumes are stitched together and rendered.

The main problems encountered so far are:

- Surfaces in areas where sroll layers are close together are not identified.
- Neither algorithm guarantees that a single surface is all from the same layer.
- When stitching surfaces together, rules need to be implemented prevent re-introduction of intersection. 

The general approach towards development is to put together a working pipeline, then refine the pipeline stages. The philsophy behind this work and it's relationship to other work is one of solution-space exploration - there are a large range of potential methods that can be used for rapidly reading the Herculanium Papyri, and exploring new and alternative avenues is beneficial for all who are working on this. Personally, it is enjoyable and informative to begin a new piece of work by striking out from scratch, at the same time as learning about the progress already made by others.  

The main files in this repository are:

- report.pdf - month 1 report describing this work
- report2.pdf - month 2 report

- *.png - some of the images that appear in the reports

- scrollprocess.py - simple tkinter user interface for viewing x,y-plane slices, quickly moving up and down in the z-plane, and invoking processing operations

- pipeline.sh - calls scrollprocess, holefiller, reander, jigsaw on a single specified 512x512x512 volume

- scrollprocess.c - contains processing functions used by scrollprocess.py. Compiles to a DLL, loaded by scrollprocess.py. Also compiles to a standalone executable to take part in the pipeline. Implements FAFF described in report.pdf

- holefiller.c - implementation of DAFF described in report.pdf (updated description in report2.pdf)

- neighbours.py - stitches together neighbouring-volume surfaces

- jigsaw.py - stitches together in-volume surface patches by starting with the largest and trying to fit successively smaller pieces in turn.
 
- neighbours_in_vol.py - Older (not very good) algorithm that stitches together in-volume surface patches

- crop.py - crop an x,y,z CSV file

- makevtk.py - turn an x,y,z CSV file into a VTK file that can be loaded into Slicer 3D

- fill3d.cpp - Experimental Hashlife 3D flood fill. Not currently used in the pipeline because it didn't outperform conventional breadth-first flood-fill, but may be useful later.

The subfolder simpaper contains programs related to in-palce growing of flat sheets on a surface.
  
