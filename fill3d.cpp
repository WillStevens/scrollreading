#include <assert.h>

#include <string.h>
#include <stdio.h>
#include <time.h>

#include <cstdint>

#include "tiffio.h"

#define VISITED_FLAG 0x80000000

#define EMPTY_SPACE 0
#define BOUNDARY 1
#define FILLING 2

#define CALCINDEX_CALC	 0
#define CALCINDEX_CALC2	 1

typedef struct otNode
{
	unsigned level;
	uint32_t calc[2];

	union
	{
		uint32_t leaf;
		uint32_t children[2][2][2];
	} u;

	uint32_t next; // for use in hash and also in freeList
} otNode;


typedef otNode *(*calcFuncPtr)(int x, int y, int z, otNode *o);


typedef struct
{
  int x,y,z;
} vector;

vector dirVectorLookup[6] =
{
  {0,1,0},
  {1,0,0},
  {0,-1,0},
  {-1,0,0},
  {0,0,1},
  {0,0,-1},
};

#define ALLOC_TABLE_SIZE 15000000
#define GC_THRESHHOLD 100000

#define EOL_INDEX ALLOC_TABLE_SIZE
#define EOL_PTR (ptrBase+EOL_INDEX)

otNode *ptrBase;
otNode *allocTable;

otNode *freeList;
unsigned freeCount;

#define HASH_SIZE 1999991

otNode **hashTable;

inline
void destroyNode(otNode *o)
{
	o->next = freeList-ptrBase;
	freeList = o;
	freeCount++;
}

void markTree(otNode *o)
{	
	if (o!=EOL_PTR && !(o->level & VISITED_FLAG))
	{
		o->level |= VISITED_FLAG;
		
		if (o->level != VISITED_FLAG) // not level 0
		{
			markTree(o->calc[0]+ptrBase);
			markTree(o->calc[1]+ptrBase);

			markTree(o->u.children[0][0][0]+ptrBase);
			markTree(o->u.children[0][0][1]+ptrBase);
			markTree(o->u.children[0][1][0]+ptrBase);
			markTree(o->u.children[0][1][1]+ptrBase);
			markTree(o->u.children[1][0][0]+ptrBase);
			markTree(o->u.children[1][0][1]+ptrBase);
			markTree(o->u.children[1][1][0]+ptrBase);
			markTree(o->u.children[1][1][1]+ptrBase);
		}
	}
}

void checkForMarked(otNode *o)
{
	if (o!=EOL_PTR)
		if (o->level & VISITED_FLAG)
		{
		    printf("Found marked node at level %d\n",o->level & ~VISITED_FLAG);
		}
		else if (o->level != 0)
		{
			checkForMarked(o->calc[0]+ptrBase);
			checkForMarked(o->calc[1]+ptrBase);
			
			checkForMarked(o->u.children[0][0][0]+ptrBase);
			checkForMarked(o->u.children[0][0][1]+ptrBase);
			checkForMarked(o->u.children[0][1][0]+ptrBase);
			checkForMarked(o->u.children[0][1][1]+ptrBase);
			checkForMarked(o->u.children[1][0][0]+ptrBase);
			checkForMarked(o->u.children[1][0][1]+ptrBase);
			checkForMarked(o->u.children[1][1][0]+ptrBase);
			checkForMarked(o->u.children[1][1][1]+ptrBase);
		}
}

void removeUnmarked(void)
{
	unsigned tested = 0,marked = 0;

	printf("removeUnmarked freeCount at start:%d\n",freeCount);
	
	for(unsigned i = 0; i<HASH_SIZE; i++)
	{
		otNode *ptr = hashTable[i];
		otNode *prev = EOL_PTR;
		while(ptr != EOL_PTR)
		{
			tested++;
			if (ptr->level & VISITED_FLAG)
			{
				//printf("a\n");
				marked++;
				ptr->level &= ~VISITED_FLAG;
			
				prev = ptr;
				
				ptr = ptr->next+ptrBase;
			}
			else if (prev != EOL_PTR)
			{
				//printf("b\n");
				prev->next = ptr->next;

				destroyNode(ptr);
				
				ptr = prev->next+ptrBase;
			}
			else
			{
				//printf("c\n");
				hashTable[i] = ptr->next+ptrBase;

				destroyNode(ptr);
				
				ptr = hashTable[i];
			}
			
		}
	}
	
	printf("removeUnmarked checked %d and found %d marked\n",tested,marked);
	printf("removeUnmarked freeCount at end:%d\n",freeCount);
}

// Garbage collect by traversing the tree, including all calc nodes, marking
// everything as visited, then going through the hash table and removing anything
// that is not visited
void garbageCollect(otNode *o)
{
	printf("GC markTree\n");
	markTree(o);
	printf("GC removeUnmarked\n");
	removeUnmarked();
	//checkForMarked(o);
}

void initTables(void)
{
    allocTable = new otNode[ALLOC_TABLE_SIZE];
    hashTable = new otNode *[HASH_SIZE];
    ptrBase = allocTable;
   
	for(unsigned i = 0; i<HASH_SIZE; i++)
	{
		hashTable[i] = EOL_PTR;
	}

	for(unsigned i = 0; i<ALLOC_TABLE_SIZE-1; i++)
	{
		allocTable[i].next = &(allocTable[i+1])-ptrBase;
	}

	allocTable[ALLOC_TABLE_SIZE-1].next = EOL_INDEX;

	freeList = &allocTable[0];
	freeCount = ALLOC_TABLE_SIZE;
}

inline
unsigned hashFunction(otNode *o)
{
	if (o->level == 0)
	{
		return o->u.leaf;
	}
	else
	{
		return ((((o->u.children[0][0][0]))<<0) +
				(((o->u.children[0][0][1]))<<1) +
				(((o->u.children[0][1][0]))<<2) +
				(((o->u.children[0][1][1]))<<3) +
				(((o->u.children[1][0][0]))<<4) +
				(((o->u.children[1][0][1]))<<5) +
				(((o->u.children[1][1][0]))<<6) +
				(((o->u.children[1][1][1]))<<7))
		        %HASH_SIZE;
	}
}

inline
otNode *addToHash(otNode *o, unsigned index)
{
	o->next = hashTable[index]-ptrBase;
	return hashTable[index] = o;
}

inline
bool compare(otNode *o1, otNode *o2)
{
	if (o1->level == o2->level)
	{
		if (o1->level == 0)
		{
			return o1->u.leaf == o2->u.leaf;
		}
		else
		{
			return o1->u.children[0][0][0]==o2->u.children[0][0][0] &&
			       o1->u.children[0][0][1]==o2->u.children[0][0][1] &&
				   o1->u.children[0][1][0]==o2->u.children[0][1][0] &&
			       o1->u.children[0][1][1]==o2->u.children[0][1][1] &&
			       o1->u.children[1][0][0]==o2->u.children[1][0][0] &&
			       o1->u.children[1][0][1]==o2->u.children[1][0][1] &&
				   o1->u.children[1][1][0]==o2->u.children[1][1][0] &&
			       o1->u.children[1][1][1]==o2->u.children[1][1][1];
//			return !memcmp(&o1->u,&o2->u,8*sizeof(otNode *));
		}
	}
	else
	{
		return false;
	}
}

inline
otNode *hashLookup(otNode *o, unsigned index)
{
	otNode *ptr = hashTable[index];

	while(ptr != EOL_PTR)
	{
		if (compare(o,ptr))
		{
			return ptr;
		}

		ptr = ptr->next+ptrBase;
	}

	return EOL_PTR;
}

// Check whether a newly created node is already in the hash
// if so then destroy o and return the one from the hash
// otherwise add o to the hash and return it
otNode *hashCheck(otNode *o)
{
	unsigned index = hashFunction(o);

	otNode *tmp = hashLookup(o,index);

	if (tmp != EOL_PTR)
	{
		destroyNode(o);
		return tmp; 
	}
	else
	{
		return addToHash(o,index);
	}
}

void checkForDuplicates(void)
{
	unsigned levelCount[256];
	
	for(unsigned i = 0; i<256; i++)
		levelCount[i] = 0;
	
	for(unsigned i = 0; i<HASH_SIZE; i++)
	{
		otNode *ptr = hashTable[i];

		int j = 0;
		
		while(ptr != EOL_PTR)
		{
			levelCount[ptr->level]++;
			
        	unsigned index = hashFunction(ptr);

			if (hashLookup(ptr,index) != ptr)
			{
				printf("Duplicate level %d node found hashTable[%d] item %d\n",ptr->level,i,j);
			}

			ptr = ptr->next+ptrBase;
			j++;
		}
	}

	for(unsigned i = 0; i<256; i++)
		if (levelCount[i] != 0)
		    printf("Level %d count=%d\n",i,levelCount[i]);
}

unsigned getValueAtLocation(otNode *o, int x, int y, int z)
{
	switch(o->level)
	{
	case 0:
		return o->u.leaf;	
	case 1:
		return ptrBase[o->u.children[x][y][z]].u.leaf;
	case 2:
		return ptrBase[ptrBase[o->u.children[x/2][y/2][z/2]].u.children[x&1][y&1][z&1]].u.leaf;
	case 3:
		return ptrBase[ptrBase[ptrBase[o->u.children[x/4][y/4][z/4]].u.children[(x/2)&1][(y/2)&1][(z/2)&1]].u.children[x&1][y&1][z&1]].u.leaf;
	default:
		{
			int d = (1<<(o->level-1));

			int xo = !(x<d);
			int yo = !(y<d);
			int zo = !(z<d);

			return getValueAtLocation(&ptrBase[
				o->u.children[xo][yo][zo]],
				x%d,
				y%d,
				z%d);
		}
		break;
	}
}

otNode *createNode(unsigned level, bool populated)
{
	otNode *rval = freeList;
	freeList = freeList->next+ptrBase;
	freeCount--;

	if (freeList == EOL_PTR)
	{
		assert(0);
	}

	rval->level = level;
	rval->calc[0] = rval->calc[1] = EOL_INDEX;
	rval->next = EOL_INDEX;
	
	switch(level)
	{
	case 0:
		rval->u.leaf = 0;
		break;
	default:
		{
			otNode *c = populated?hashCheck(createNode(rval->level-1,populated)):EOL_PTR;
			
			for(int i = 0; i<2; i++)
				for(int j = 0; j<2; j++)
					for(int k = 0; k<2; k++)
					{
						rval->u.children[i][j][k] = c-ptrBase;
					}
		}
		break;
	}

	return rval;
}

// create a duplicate node with everything the same
otNode *duplicateNode(otNode *o)
{
	otNode *rval = createNode(o->level,false);

	*rval = *o;

	return rval;
}

otNode *hashCheckValue(unsigned v)
{
	otNode *o = createNode(0,false);
	o->u.leaf = v;

	return hashCheck(o);
}

// Make a mutable copy of the subtree in which the block resides, then
// make the change
otNode *setValueAtLocation(otNode *o, int x, int y, int z, unsigned v)
{
	otNode *rval = duplicateNode(o);
	rval->calc[0] = rval->calc[1] = EOL_INDEX;
	rval->next = EOL_INDEX;
	
	switch(o->level)
	{
	case 0:
		rval->u.leaf = v;
		break;	
	default:
		{
			int d = (1<<(o->level-1));

			int xo = ((x<d)?0:1);
			int yo = ((y<d)?0:1);
			int zo = ((z<d)?0:1);
			
			rval->u.children[xo][yo][zo] = setValueAtLocation(&ptrBase[rval->u.children[xo][yo][zo]],
				x-xo*d,
				y-yo*d,
				z-zo*d,
				v) - ptrBase;
		}
		break;
	}

	return hashCheck(rval);
}

// Make a mutable copy of the subtree in which the block resides, then
// make the change
// x must be equal to 0 mod 2
otNode *setValuesAtLocation(otNode *o, int x, int y, int z, unsigned v1, unsigned v2)
{
	otNode *rval = duplicateNode(o);
	rval->calc[0] = rval->calc[1] = EOL_INDEX;
	rval->next = EOL_INDEX;
	
	switch(o->level)
	{
	case 0:
		assert(0);
		break;	
	case 1:
		{
			int d = 1;

			int xo = 0;
			int yo = y;
			int zo = z;
			
			rval->u.children[0][y][z] = setValueAtLocation(&ptrBase[rval->u.children[0][y][z]],
				0,0,0,v1) - ptrBase;
			rval->u.children[1][y][z] = setValueAtLocation(&ptrBase[rval->u.children[1][y][z]],
				0,0,0,v2) - ptrBase;
		}
		break;	
	default:
		{
			int d = (1<<(o->level-1));

			int xo = ((x<d)?0:1);
			int yo = ((y<d)?0:1);
			int zo = ((z<d)?0:1);
			
			otNode *c = &ptrBase[rval->u.children[xo][yo][zo]];
			
			if (c->level == 0)
			{
				printf("Incorrect level 0 found at level %d\n",o->level);
				assert(0);
			}
			
			rval->u.children[xo][yo][zo] = setValuesAtLocation(&ptrBase[rval->u.children[xo][yo][zo]],
				x-xo*d,
				y-yo*d,
				z-zo*d,
				v1,v2) - ptrBase;
		}
		break;
	}

	return hashCheck(rval);
}

otNode *processCell(int x, int y, int z, otNode *o)
{
	unsigned v = getValueAtLocation(o,x,y,z);

	bool changed = false;

	if (v==EMPTY_SPACE)
	{
		for(unsigned i = 0; i<6; i++)
		{
			vector p = dirVectorLookup[i];

			unsigned vn = getValueAtLocation(o,x+p.x,y+p.y,z+p.z);

			if (vn==FILLING)
			{
				v = FILLING;
			}
		}
	}

    return hashCheckValue(v);	
}
		
otNode *otLevel2NodeCalc(otNode *o, calcFuncPtr calcFunc)
{
	otNode *rval = createNode(1,false);

	for(int ii = 0; ii<2; ii++)
		for(int jj = 0; jj<2; jj++)
			for(int kk = 0; kk<2; kk++)
			{
				rval->u.children[ii][jj][kk] = calcFunc(ii+1,jj+1,kk+1,o)-ptrBase;
			}

	return hashCheck(rval);
}

otNode *populateGapNode(otNode *o, int i2, int j2, int k2, int i1, int j1, int k1)
{
	otNode *rval = createNode(o->level-2,false);

	for(int i = 0; i<2; i++)
		for(int j = 0; j<2; j++)
			for(int k = 0; k<2; k++)
			{
				int x = 1+i2*2+i1*2+i;
				int y = 1+j2*2+j1*2+j;
				int z = 1+k2*2+k1*2+k;

				rval->u.children[i][j][k] = 
					ptrBase[ptrBase[o->u.children[(x&4)>>2][(y&4)>>2][(z&4)>>2]]
					 .u.children[(x&2)>>1][(y&2)>>1][(z&2)>>1]]
					 .u.children[x&1][y&1][z&1];
			}
	
	return hashCheck(rval);
}

otNode *makeTempChild(otNode *o, int i1, int j1, int k1)
{
	otNode *rval = createNode(o->level-1,false);

	for(int i = 0; i<2; i++)
		for(int j = 0; j<2; j++)
			for(int k = 0; k<2; k++)
			{
				rval->u.children[i][j][k] = populateGapNode(o,i1,j1,k1,i,j,k)-ptrBase;
			}

	return hashCheck(rval);
}

otNode *makeIntermed(otNode *o, int i1, int j1, int k1)
{
	otNode *rval = createNode(o->level-1,false);
	
	for(int i=0;i<2;i++)
		for(int j=0;j<2;j++)
			for(int k=0;k<2;k++)
			{
				int x = i1+i;
				int y = j1+j;
				int z = k1+k;

				rval->u.children[i][j][k] = 
					ptrBase[o->u.children[x>>1][y>>1][z>>1]]
 					.u.children[x&1][y&1][z&1];
			}

	return hashCheck(rval);
}

otNode *makeInterChild(otNode **intermed,int i1, int j1, int k1)
{
	otNode *rval = createNode((*intermed)->level+1,false);
	
	for(int i=0;i<2;i++)
		for(int j=0;j<2;j++)
			for(int k=0;k<2;k++)
			{
				int x = i + i1;
				int y = j + j1;
				int z = k + k1;

				rval->u.children[i][j][k] = (*(intermed + 3*(3*x+y) + z))-ptrBase; 
			}

	return hashCheck(rval);
}

// returns a pointer to a node one level down and centred
otNode *otNodeCalc(otNode *o, calcFuncPtr calcFunc, unsigned calcIndex)
{
	otNode *rval;

	if (o->calc[calcIndex] == EOL_INDEX)
	{
		if (o->level==2)
		{
			o->calc[calcIndex] = otLevel2NodeCalc(o,calcFunc)-ptrBase;
		}
		else if (calcIndex == CALCINDEX_CALC2)
		{
			rval = createNode(o->level-1,false); 

			otNode *intermed[3][3][3];

			for (int i=0; i<3; i++)
				for(int j=0; j<3; j++)
					for(int k=0; k<3; k++)
					{
						intermed[i][j][k] = makeIntermed(o,i,j,k);
						intermed[i][j][k] = otNodeCalc(intermed[i][j][k],calcFunc,calcIndex);
					}

			for(int i=0; i<2; i++)
				for(int j=0; j<2; j++)
					for(int k=0; k<2; k++)
					{				
						rval->u.children[i][j][k] = makeInterChild(&intermed[0][0][0],i,j,k)-ptrBase;
						rval->u.children[i][j][k] = otNodeCalc(&ptrBase[rval->u.children[i][j][k]],calcFunc,calcIndex)-ptrBase;
					}

			o->calc[calcIndex] = hashCheck(rval)-ptrBase;
		}
		else
		{
			rval = createNode(o->level-1,false);

			for (int i=0; i<2; i++)
				for(int j=0; j<2; j++)
					for(int k=0; k<2; k++)
					{
						rval->u.children[i][j][k] = makeTempChild(o,i,j,k)-ptrBase;
						rval->u.children[i][j][k] = otNodeCalc(&ptrBase[rval->u.children[i][j][k]],calcFunc,calcIndex)-ptrBase;
					}

			o->calc[calcIndex] = hashCheck(rval)-ptrBase;
		}

		return &ptrBase[o->calc[calcIndex]];
	}
	else
	{
		return &ptrBase[o->calc[calcIndex]];
	}
}

otNode *expandUniverse(otNode *o)
{
	otNode *rval = createNode(o->level+1,false);

	for(int i = 0; i<2; i++)
		for(int j = 0; j<2; j++)
			for(int k = 0; k<2; k++)
			{
				otNode *tmp = createNode(o->level,true);
				tmp->u.children[1-i][1-j][1-k] = o->u.children[i][j][k];
				rval->u.children[i][j][k] = hashCheck(tmp)-ptrBase;
			}

	return hashCheck(rval);
}

otNode *singleStep(otNode *eg, unsigned &iters, bool speedUp)
{
	calcFuncPtr calcFunc;

	calcFunc = processCell;

	if (speedUp)
	{
		eg = otNodeCalc(expandUniverse(eg),calcFunc,CALCINDEX_CALC2);

		iters += 1<<(eg->level-2);
	}
	else
	{
		eg = otNodeCalc(expandUniverse(eg),calcFunc,CALCINDEX_CALC);

		iters+=1;
	}
	
	if (freeCount < GC_THRESHHOLD)
	{
		printf("GC\n");
		garbageCollect(eg);
	}

	return eg;
}

void printUniverse(otNode *o)
{
	unsigned size = 1<<(o->level);
	
	for(int z=0; z<size; z++)
	{
		for(int y=0; y<size; y++)
		{
			for(int x=0; x<size; x++)
				printf("%d",getValueAtLocation(o,x,y,z));
			printf("\n");
		}
		printf("---\n");
	}
}


int main(int argc, char *argv[])
{		
	unsigned iters = 0;

	initTables();

	printf("Node size (bytes):%d\n",sizeof(otNode));
	
	printf("Started:%d\n",time(NULL));

	unsigned level = 9;
	unsigned size = 1<<level;

	printf("freeCount:%d\n",freeCount);
	
	otNode *eg = hashCheck(createNode(level,true));

	printf("freeCount:%d\n",freeCount);

	eg = setValueAtLocation(eg,5,5,5,2);

	garbageCollect(eg);
	
	printf("About to load\n");

	
	for(uint32_t m = 0; m<size; m++)
	{
		char fname[100];
		sprintf(fname,"../construct/e002/e002_0%d.tif",5800+m);
		TIFF *tif = TIFFOpen(fname,"r");

	
		if (tif)
		{
			uint32_t imagelength;
			tdata_t buf;
			uint32_t row;
			TIFFGetField(tif,TIFFTAG_IMAGELENGTH,&imagelength);
			uint32_t linesize = TIFFScanlineSize(tif);
			buf = _TIFFmalloc(linesize);
			for(row=0;row<(imagelength<size?imagelength:size);row++)
			{
				TIFFReadScanline(tif,buf,row,0);
				for(int i=0; i<size; i+=2)
				{
					uint16_t v1 = ((uint16_t *)buf)[i];
					uint16_t v2 = ((uint16_t *)buf)[i+1];
					eg = setValuesAtLocation(eg,i,row,m,v1<28000?1:0,v2<28000?1:0);

					if (freeCount < GC_THRESHHOLD)
					{
						garbageCollect(eg);
					}
				}
			}
			_TIFFfree(buf);
		}
	
		TIFFClose(tif);
	}

//	printUniverse(eg);	

	printf("GC\n");
	garbageCollect(eg);
	printf("done GC\n");

	checkForDuplicates();
	printf("freeCount after load and GC:%d ",freeCount);
	printf("\n");

	eg = setValueAtLocation(eg,5,5,5,2);
	
/*	for(int x = 4; x<12; x++)
		for(int y = 4; y<12; y++)
			for(int z = 4; z<12; z++)
			{
				if (x==4 || x==11 || y==4 || y==11 || z==4 || z==11)
					eg = setValueAtLocation(eg,x,y,z,1);
			}
*/	
//	printUniverse(eg);
	printf("Started filling:%d\n",time(NULL));
		
	eg = singleStep(eg,iters,true);
//	eg = singleStep(eg,iters,false);
    printf("Finished:%d\n",time(NULL));
	printf("Iterations:%d\n",iters);
	
//	printUniverse(eg);	

	printf("freeCount:%d\n",freeCount);

	eg = singleStep(eg,iters,true);

    printf("Finished:%d\n",time(NULL));
	printf("Iterations:%d\n",iters);
	
	printf("freeCount before GC:%d\n",freeCount);

	garbageCollect(eg);

	printf("freeCount after GC:%d\n",freeCount);
	
	return 0;
}
