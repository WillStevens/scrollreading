#include "bigpatch.cpp"

std::vector<gridPoint> gridPoints;

void LoadPointSet(const char *fname,int patch)
{
	FILE *f = fopen(fname,"r");
	
	if (f)
	{
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
			gridPoints.push_back(gridPoint(x,y,px,py,pz,patch));
		}
	  
	    fclose(f);
	}
	else
	{
		printf("Unable to open file %s\n",fname);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 4)
	{
		printf("Usage: randombigpatchpoint <patch> <rn1> <rn2>\n");
		printf("Patch is a bigpatch, rn1 and rn2 are randomly generated unsigned ints used for selecting random point\n");
		exit(-1);
	}

  BigPatch *bp = OpenBigPatch(argv[1]);

  if (bp)
  {	  
    unsigned rn1 = atoi(argv[2]);
    unsigned rn2 = atoi(argv[3]);
  
    gridPoint gp = SelectRandomPoint(bp,rn1,rn2);
  
    CloseBigPatch(bp);
 
    printf("%f %f %f %f %f %d\n",std::get<0>(bp),std::get<1>(bp),std::get<2>(bp),std::get<3>(bp),std::get<4>(bp),std::get<5>(bp));
    return 0;
  }
  else
  {
    return -1;
  }	
}