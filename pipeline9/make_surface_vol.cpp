/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
  
 Released under GNU Public License V3
*/

#include <string> 
#include <unordered_map>
#include <algorithm>
#include <numeric>

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

#include "tiffio.h"

#include "zarr_4_b.c"
#include "zarr_svol.c"

#include "bigpatch.h"


#define ROUND(x)  (x>=0 ? ((x)-(int)(x)<0.5?(int)(x):((int)(x))+1) : ((x)-(int)(x)>=-0.5?(int)(x):((int)(x))-1) )

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

int minx=-1,maxx=-1,miny=-1,maxy=-1;
int SHEET_SIZE_X=0;
int SHEET_SIZE_Y=0;

#define FILENAME_LENGTH 300
#define FOLDERNAME_LENGTH 256
char targetDir[FOLDERNAME_LENGTH]="";
char imageDir[FOLDERNAME_LENGTH]="";
unsigned zOffset=0;


// Sorts points so that points falling in the same (chunk_size x chunk_size)
// spatial cell end up contiguous in the vector. This minimizes how many times
// a given zarr chunk needs to be loaded/buffered.
void sortByChunk(std::vector<gridPoint>& points, float chunk_size)
{
    const size_t n = points.size();

    // Precompute integer chunk coordinates once (avoid recomputing in comparator)
    std::vector<std::pair<long long,long long>> chunkIdx(n);
    for (size_t i = 0; i < n; ++i)
    {
        float x = std::get<0>(points[i]);
        float y = std::get<1>(points[i]);
        chunkIdx[i] = { static_cast<long long>(std::floor(x / chunk_size)),
                        static_cast<long long>(std::floor(y / chunk_size)) };
    }

    // Sort a permutation of indices instead of the tuples directly, so we only
    // move each gridPoint once at the end.
    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), 0);

    std::sort(order.begin(), order.end(), [&](size_t a, size_t b)
    {
        if (chunkIdx[a].first  != chunkIdx[b].first)
            return chunkIdx[a].first  < chunkIdx[b].first;
        if (chunkIdx[a].second != chunkIdx[b].second)
            return chunkIdx[a].second < chunkIdx[b].second;
        // Tie-break inside a chunk for a bit of extra locality
        if (std::get<0>(points[a]) != std::get<0>(points[b]))
            return std::get<0>(points[a]) < std::get<0>(points[b]);
        return std::get<1>(points[a]) < std::get<1>(points[b]);
    });

    std::vector<gridPoint> sorted;
    sorted.reserve(n);
    for (size_t idx : order)
        sorted.push_back(std::move(points[idx]));
    points = std::move(sorted);
}

void WriteZarrSlice(ZARR_4 *volZarr, std::string &binFile, float zpos, ZARR_svol *svolZarr, bool getMins, bool &first, float &minux, float &minuy)
{	
    printf("Loading points\n");
    std::vector<gridPoint> gridPoints;
	{
		FILE *f = fopen(binFile.c_str(),"r");
		
		gridPointStruct p;
		fseek(f,0,SEEK_END);
		long fsize = ftell(f);
		fseek(f,0,SEEK_SET);
  
		// input in x,y,z order 
		while(ftell(f)<fsize)
		{
			fread(&p,sizeof(p),1,f);
			float x,y,px,py,pz;
			x=p.x;y=p.y;px=p.px;py=p.py;pz=p.pz;
			gridPoints.push_back(gridPoint(x,y,px,py,pz,0));
		}
		
		fclose(f);
	}

    printf("Sorting %d points\n",(int)gridPoints.size());
	sortByChunk(gridPoints,128);

    printf("Writing %d points\n",(int)gridPoints.size());
	for(auto const &gp : gridPoints)
	{
		float ux = std::get<0>(gp);
		float uy = std::get<1>(gp);
		float vx = std::get<2>(gp);
		float vy = std::get<3>(gp);
		float vz = std::get<4>(gp);

		if (getMins)
		{
			if (ux < minux || first) minux = ux;
			if (uy < minuy || first) minuy = uy;
			first = false;
		}
		else
		{
			uint8_t v = ZARRRead_4(volZarr,vz,vy,vx);

			ZARRWrite_svol(svolZarr,zpos,ROUND(uy-minuy),ROUND(ux-minux),v);
		}
	}
}

/*
 * Checks if filename matches "<name>_n.bin".
 * On success, returns true and writes the parsed integer to *out_n.
 * On failure, returns false.
 */
bool parse_name_n_bin(const char *filename, int *out_n) {
    size_t len = strlen(filename);
    const char *suffix = ".bin";
    size_t suffix_len = strlen(suffix);

    /* Must end with ".bin" */
    if (len <= suffix_len || strcmp(filename + len - suffix_len, suffix) != 0) {
        return false;
    }

    /* Find the last underscore before ".bin" */
    const char *dot_pos = filename + len - suffix_len; /* points at '.' */
    const char *underscore_pos = NULL;

    for (const char *p = dot_pos - 1; p >= filename; p--) {
        if (*p == '_') {
            underscore_pos = p;
            break;
        }
    }

    if (underscore_pos == NULL) {
        return false;
    }

    /* Everything between underscore_pos+1 and dot_pos must be digits, and non-empty */
    const char *num_start = underscore_pos + 1;
    if (num_start >= dot_pos) {
        return false; /* no digits between '_' and ".bin" */
    }

    for (const char *p = num_start; p < dot_pos; p++) {
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
    }

    /* There must be at least one character for <name> before the underscore */
    if (underscore_pos == filename) {
        return false;
    }

    /* Parse the integer */
    char num_buf[32];
    size_t num_len = (size_t)(dot_pos - num_start);
    if (num_len >= sizeof(num_buf)) {
        return false; /* number too long, unlikely but guard anyway */
    }
    memcpy(num_buf, num_start, num_len);
    num_buf[num_len] = '\0';

    *out_n = atoi(num_buf);
    return true;
}

typedef std::vector<std::pair<std::string,int>> fileNumberList;

bool GetBinFileList(const char * path,fileNumberList &list)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip "." and ".." (and other non-regular-ish entries if you want to be stricter) */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        int n;
        if (parse_name_n_bin(entry->d_name, &n)) {
            char full_path[4096];
            int written = snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            if (written < 0 || (size_t)written >= sizeof(full_path)) {
                fprintf(stderr, "Path too long, skipping: %s\n", entry->d_name);
                continue;
            }

            list.push_back({std::string(full_path),n});
        }
    }

    closedir(dir);
    return true;
}

int main(int argc, char *argv[])
{
	if (argc != 4 )
	{
		printf("Usage: make_surface_vol <volume zarr> <bin folder> <output zarr>\n");
		printf("Create a u8 surface zarr from a stack of .bin files");
		return -1;
	}

	ZARR_4 *volZarr = ZARROpen_4(argv[1]);
	ZARR_svol *svolZarr = ZARROpen_svol(argv[3]);

	float minux = 0, minuy = 0;
	bool first = true;

	fileNumberList binFileList;
	
	if (!GetBinFileList(argv[2],binFileList))
	{
		printf("Couldn't get bin file list\n");
		exit(-1);
	}
	
	if (binFileList.size()==0)
	{
		printf("Bin file list is empty\n");
		exit(-1);
	}
	
	for (auto &bn : binFileList)
	{
		WriteZarrSlice(volZarr,bn.first,bn.second,svolZarr,true,first,minux,minuy);
	}
	
	for (auto &bn : binFileList)
	{
		printf("Slice %d\n",bn.second);
		WriteZarrSlice(volZarr,bn.first,bn.second,svolZarr,false,first,minux,minuy);
	}
	
	ZARRClose_svol(svolZarr);
	ZARRClose_4(volZarr);
	
	return 0;
}
