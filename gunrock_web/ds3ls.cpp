#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <sstream>

//#include "StringUtils.h"
#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

// Use this function with std::sort for directory entries
bool compareByName(const dir_ent_t& a, const dir_ent_t& b) {
  return std::strcmp(a.name, b.name) < 0;
}

vector<string> splitPath(const string &path) {
  vector<string> components;
  stringstream ss(path);
  string part;
  while(getline(ss, part, '/')) {
    if(!part.empty()) {
      components.push_back(part);
    }
  }
  return components;
}

void printDirectoryContents(int inodeNumber, LocalFileSystem* fileSystem) {
  inode_t inode;
  if((fileSystem->stat(inodeNumber, &inode) != 0) || (inode.type != UFS_DIRECTORY)) {
    cerr << "Directory not found" << endl;
    return;
  }
  vector<dir_ent_t> entries;
  char* buffer = new char[inode.size];
  dir_ent_t* dirEntry =(dir_ent_t*)(buffer);
  fileSystem->read(inodeNumber, buffer, inode.size);
  int totalEntries = inode.size / sizeof(dir_ent_t);
  for(int i = 0; i < totalEntries; i++) {
    entries.push_back(dirEntry[i]);
  }
  delete[] buffer;
  sort(entries.begin(), entries.end(), compareByName);
  for(const auto& entry : entries) {
    cout << entry.inum << "\t" << entry.name << endl;
  }
}

int main(int argc, char *argv[]) {
  if(argc != 3) {
    cerr << argv[0] << ": diskImageFile directory" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img /a/b" << endl;
    return 1;
  }

  // parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string directory = string(argv[2]);
  vector<string> pathComponents = splitPath(directory);
  int currentInodeNumber = UFS_ROOT_DIRECTORY_INODE_NUMBER;
  for(const auto& component : pathComponents) {
    inode_t inode;
    if((fileSystem->stat(currentInodeNumber, &inode) != 0) ||(inode.type != UFS_DIRECTORY)) {
      cerr << "Directory not found" << endl;
      delete fileSystem;
      delete disk;
      return 1;
    }
    bool directoryFound = false;
    char* buffer = new char[inode.size];
    fileSystem->read(currentInodeNumber, buffer, inode.size);
    dir_ent_t* dirEntry =(dir_ent_t*)(buffer);
    int totalEntries = inode.size / sizeof(dir_ent_t);
    for(int i = 0; i < totalEntries; i++) {
      if(component == dirEntry[i].name) {
        currentInodeNumber = dirEntry[i].inum;
        directoryFound = true;
        break;
      }
    }
    delete[] buffer;
    if(!directoryFound) {
      cerr << "Directory not found" << endl;
      delete fileSystem;
      delete disk;
      return 1;
    }
  }
  inode_t finalInode;
  if(fileSystem->stat(currentInodeNumber, &finalInode) != 0) {
    cerr << "Directory not found" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }

  if(finalInode.type == UFS_DIRECTORY) {
    printDirectoryContents(currentInodeNumber, fileSystem);
  } else if(finalInode.type == UFS_REGULAR_FILE) {
    cout << currentInodeNumber << "\t" << pathComponents.back() << endl;
  } else {
    cerr << "Directory not found" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
  delete fileSystem;
  delete disk;
  return 0;
}