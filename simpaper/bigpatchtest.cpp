#include "bigpatch.cpp"

int main(void)
{
  BigPatch *bp = OpenBigPatch("bigpatchtest.bp");
  
  std::vector<gridPoint> gridPoints;
  
  gridPoints.push_back(gridPoint(0,0,0,0,0));
  gridPoints.push_back(gridPoint(0,1,1,0,0));
  gridPoints.push_back(gridPoint(1,0,0,1,0));
  gridPoints.push_back(gridPoint(1,1,0,0,1));
  
  WritePatchPoints(bp,chunkIndex(0,0,0),gridPoints);
  
  CloseBigPatch(bp);
 
  return 0; 
}