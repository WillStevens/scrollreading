
getOMEZarr() {
  local cut=$1
  local vol=$2

#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/.zarray

  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/0/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/1/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/2/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/3/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/4/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/5/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/6/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/7/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/8/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/9/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/10/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/11/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/12/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/13/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/14/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/15/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/16/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/17/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/18/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/19/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/20/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/21/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/22/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/23/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/24/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/25/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/26/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/27/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/28/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/29/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/30/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/31/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/32/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/33/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/34/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/35/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/36/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/37/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/38/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/39/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/40/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/41/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/42/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/43/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/44/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/45/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/46/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/47/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/48/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/49/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/50/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/51/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/52/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/53/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/54/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/55/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/56/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/57/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/58/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/59/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/60/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/61/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/62/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/63/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/64/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/65/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/66/
#  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/67/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/68/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/69/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/70/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/71/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/72/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/73/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/74/
  wget -r -np -nH --cut-dirs=$cut -R "index.html*" $vol/0/75/

}


volume_prefix=https://dl.ash2txt.org/full-scrolls/Scroll4/PHerc1667.volpkg/volumes_zarr/20231117161658.zarr
getOMEZarr 4 $volume_prefix
