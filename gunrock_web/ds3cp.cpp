#include <iostream>
#include <string>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc != 4) {
    cerr << argv[0] << ": diskImageFile src_file dst_inode" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img dthread.cpp 3" << endl;
    return 1;
  }

  // Parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string srcFile = string(argv[2]);
  int dstInode = stoi(argv[3]);
  int fd = open(srcFile.c_str(), O_RDONLY);
  if (fd < 0) {
    cerr << "Could not write to dst_file" << srcFile << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
  inode_t inode;
  int existingInode = fileSystem->stat(dstInode, &inode);
  if(existingInode == -1) {
    cerr << "Could not write to dst_file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
  vector<char> buffer;
  char tempBuffer[UFS_BLOCK_SIZE];
  ssize_t bytesRead;
  while ((bytesRead = read(fd, tempBuffer, sizeof(tempBuffer))) > 0) {
    buffer.insert(buffer.end(), tempBuffer, tempBuffer + bytesRead);
  }
  buffer.push_back('\0');
  int writeResult = fileSystem->write(dstInode, buffer.data(), buffer.size());
  if(writeResult == -EINVALIDINODE || writeResult == -EINVALIDTYPE || writeResult == -EINVALIDSIZE || writeResult == -ENOTENOUGHSPACE) {
    cerr << "Could not write to dst_file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
  delete fileSystem;
  delete disk;
  return 0;
}
