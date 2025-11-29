#0 in 1 must be 0,1 in 0
#Z=28 in 1 corresponds to 56,57 in 0 

getMissingFile() {
  local cut=$1
  local vol=$2

  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/.zarray
}

getOMEZarr() {
  local cut=$1
  local vol=$2

  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/.zattrs

  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/.zgroup

  # get it if it exists
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/meta.json

  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/.zarray
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/.zarray
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/.zarray
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/3/.zarray
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/4/.zarray
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/5/.zarray


  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/18/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/19/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/20/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/21/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/22/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/23/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/24/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/25/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/26/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/27/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/28/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/29/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/30/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/31/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/32/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/33/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/34/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/35/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/1/36/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/9/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/10/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/11/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/12/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/13/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/14/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/15/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/16/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/17/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/2/18/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/3/4/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/3/5/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/3/6/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/3/7/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/3/8/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/3/9/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/4/2/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/4/3/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/4/4/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/5/1/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/5/2/
}

mkdir scroll1.volpkg
cd scroll1.volpkg
mkdir volumes
mkdir paths
wget -r -np -nH --cut-dirs=4 -R "index.html*" https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/config.json

cd volumes

volume_prefix=https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr
getOMEZarr 4 $volume_prefix
#getMissingFile 4 $volume_prefix

volume_prefix=https://dl.ash2txt.org/community-uploads/bruniss/scrolls/s1/surfaces/s1_059_ome.zarr/
getOMEZarr 5 $volume_prefix
#getMissingFile 5 $volume_prefix
