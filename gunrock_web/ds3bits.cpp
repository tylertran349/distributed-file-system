#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;


int main(int argc, char *argv[]) {
  if (argc != 2) {
    cerr << argv[0] << ": diskImageFile" << endl;
    return 1;
  }

  // Parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  super_t superBlock;
  fileSystem->readSuperBlock(&superBlock);
  cout << "Super" << endl;
  cout << "inode_region_addr" << " " <<  superBlock.inode_region_addr << endl;
  cout << "inode_region_len" << " " << superBlock.inode_region_len << endl;
  cout << "num_inodes" << " " << superBlock.num_inodes << endl;
  cout << "data_region_addr" << " " << superBlock.data_region_addr << endl;
  cout << "data_region_len" << " " << superBlock.data_region_len << endl;
  cout << "num_data" << " " << superBlock.num_data << endl;
  cout << endl;
  unsigned char inodeBitmap[UFS_BLOCK_SIZE];
  fileSystem->readInodeBitmap(&superBlock, inodeBitmap);
  int inodeBitmapSize = (superBlock.num_inodes + 7) / 8;
  cout << "Inode bitmap" << endl;
  for(int i = 0; i < inodeBitmapSize; i++) {
    cout << (unsigned int)(inodeBitmap[i]) << " ";
  }
  cout << endl << endl;
  unsigned char dataBitmap[UFS_BLOCK_SIZE];
  fileSystem->readDataBitmap(&superBlock, dataBitmap);
  int dataBitmapSize = (superBlock.num_data + 7) / 8;
  cout << "Data bitmap" << endl;
  for(int i = 0; i < dataBitmapSize; i++) {
    cout << (unsigned int)(dataBitmap[i]) << " ";
  }
  cout << endl;
  delete fileSystem;
  delete disk;
  return 0;
}
