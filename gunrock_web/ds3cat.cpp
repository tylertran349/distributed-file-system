#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;


int main(int argc, char *argv[]) {
  if (argc != 3) {
    cerr << argv[0] << ": diskImageFile inodeNumber" << endl;
    return 1;
  }

  // Parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  int inodeNumber = stoi(argv[2]);
  inode_t inode;
  fileSystem->stat(inodeNumber, &inode);
  if(inode.type != UFS_REGULAR_FILE) {
    cerr << "Error reading file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
  cout << "File blocks" << endl;
  int numBlocks = (inode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  for(int i = 0; i < numBlocks; i++) {
    if(inode.direct[i] != 0) {
      cout << inode.direct[i] << endl;
    }
  }
  cout << endl;
  char* buffer = new char[inode.size + 1];
  cout << "File data" << endl;
  fileSystem->read(inodeNumber, buffer, inode.size);
  buffer[inode.size] = '\0';
  cout << buffer;
  delete[] buffer;
  delete fileSystem;
  delete disk;
  return 0;
}