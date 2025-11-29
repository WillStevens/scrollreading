
getOMEZarr() {
  local cut=$1
  local vol=$2

  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/.zarray

#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/32/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/33/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/34/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/35/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/36/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/37/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/38/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/39/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/40/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/41/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/42/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/43/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/44/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/45/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/46/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/47/
}


volume_prefix=https://dl.ash2txt.org/full-scrolls/Scroll4/PHerc1667.volpkg/volumes_zarr/20231117161658.zarr
getOMEZarr 4 $volume_prefix
