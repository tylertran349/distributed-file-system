#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include <cstring>
#include <cmath>
#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;


LocalFileSystem::LocalFileSystem(Disk *disk) {
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super) {
  if(super == nullptr) {
    return;
  }
  char buffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, buffer);
  memcpy(super, buffer, sizeof(super_t));
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
  if(super == nullptr || inodeBitmap == nullptr) {
    return;
  }
  disk->readBlock(super->inode_bitmap_addr, (char*)inodeBitmap);
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {

}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
  if(super == nullptr || dataBitmap == nullptr) {
    return;
  }
  disk->readBlock(super->data_bitmap_addr, (char*)dataBitmap);
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {

}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {

}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {

}

int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  inode_t parentInode;
  if(stat(parentInodeNumber, &parentInode) != 0) {
    return -EINVALIDINODE;
  }
  if(parentInode.type != UFS_DIRECTORY) {
    return -EINVALIDTYPE;
  }
  char block[UFS_BLOCK_SIZE];
  int numBlocks = (parentInode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  for(int i = 0; i < numBlocks; i++) {
    if(parentInode.direct[i] == 0) {
      continue;
    }
    disk->readBlock(parentInode.direct[i], block);
    dir_ent_t* entries = (dir_ent_t*)block;
    for(size_t j = 0; j < UFS_BLOCK_SIZE / sizeof(dir_ent_t); j++) {
      if(entries[j].inum != -1 && name == entries[j].name) {
        return entries[j].inum;
      }
    }
  }
  return -ENOTFOUND;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  if(inode == nullptr || inodeNumber < 0) {
    return -1;
  }
  super_t superBlock;
  readSuperBlock(&superBlock);
  if(inodeNumber >= superBlock.num_inodes) {
    return -1;
  }
  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  int blockNumber = superBlock.inode_region_addr + (inodeNumber / inodesPerBlock);
  int inodeOffset = (inodeNumber % inodesPerBlock) * sizeof(inode_t);
  char block[UFS_BLOCK_SIZE];
  disk->readBlock(blockNumber, block);
  memcpy(inode, block + inodeOffset, sizeof(inode_t));
  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  inode_t inode;
  if(size < 0 || size > MAX_FILE_SIZE) {
    return -EINVALIDSIZE;
  }
  if(stat(inodeNumber, &inode) != 0) {
    return -EINVALIDINODE;
  } 
  size = min(size, inode.size);
  int bytesRead = 0;
  int numBlocks = (size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  char block[UFS_BLOCK_SIZE];
  for(int i = 0; i < numBlocks && bytesRead < size; i++) {
    if(inode.direct[i] == 0) {
      continue;
    }
    int numBytesToRead = min(UFS_BLOCK_SIZE, size - bytesRead);
    disk->readBlock(inode.direct[i], block);
    memcpy((char*)buffer + bytesRead, block, numBytesToRead);
    bytesRead += numBytesToRead;
  }
  return bytesRead;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  if(parentInodeNumber < 0) {
    return -EINVALIDINODE;
  }
  if(type != UFS_REGULAR_FILE && type != UFS_DIRECTORY) {
    return -EINVALIDTYPE;
  }
  if(name.length() > DIR_ENT_NAME_SIZE) {
    return -EINVALIDNAME;
  }
  int existingInodeNumber = lookup(parentInodeNumber, name);
  inode_t existingInode;
  if(existingInodeNumber != -ENOTFOUND) {
    if(stat(existingInodeNumber, &existingInode) == 0 && existingInode.type == type) {
      return existingInodeNumber;
    }
    return -EINVALIDTYPE;
  }
  int newInodeNumber = -1;
  super_t super;
  readSuperBlock(&super);
  vector<u_int8_t> inodeBitmap(super.inode_bitmap_len * UFS_BLOCK_SIZE);
  readInodeBitmap(&super, inodeBitmap.data());
  vector<u_int8_t> dataBitmap(super.data_bitmap_len * UFS_BLOCK_SIZE);
  readDataBitmap(&super, dataBitmap.data());
  for(int i = 0; i < super.num_inodes; i++) {
    if(!(inodeBitmap.at(i / 8) & (1 << (i % 8)))) {
      inodeBitmap.at(i / 8) = inodeBitmap.at(i / 8) | (1 << (i % 8));
      newInodeNumber = i;
      break;
    }
  }
  if(newInodeNumber == -1) {
    return -ENOTENOUGHSPACE;
  }
  inode_t newInode = {};
  newInode.type = type;
  if(type == UFS_REGULAR_FILE) {
    newInode.size = 0;
  } else {
    newInode.size = 2 * sizeof(dir_ent_t);
  }
  if(type == UFS_DIRECTORY) {
    int newDirBlock = -1;
    for(int i = 0; i < super.num_data; i++) {
      if(!(dataBitmap.at(i / 8) & (1 << (i % 8)))) {
        dataBitmap.at(i / 8) = dataBitmap.at(i / 8) | (1 << (i % 8));
        newDirBlock = super.data_region_addr + i;
        break;
      }
    }
    if(newDirBlock == -1) {
      return -ENOTENOUGHSPACE;
    }
    dir_ent_t entries[2] = { {".", newInodeNumber}, {"..", parentInodeNumber} };
    disk->writeBlock(newDirBlock, entries);
    newInode.direct[0] = newDirBlock;
  }
  int inodeBlockNumber = super.inode_region_addr + (newInodeNumber / (UFS_BLOCK_SIZE / sizeof(inode_t)));
  char inodeBlock[UFS_BLOCK_SIZE];
  disk->readBlock(inodeBlockNumber, inodeBlock);
  memcpy(inodeBlock + (newInodeNumber % (UFS_BLOCK_SIZE / sizeof(inode_t))) * sizeof(inode_t), &newInode, sizeof(inode_t));
  disk->writeBlock(inodeBlockNumber, inodeBlock);
  inode_t parentInode;
  if(stat(parentInodeNumber, &parentInode) != 0 || parentInode.type != UFS_DIRECTORY) {
    return -EINVALIDINODE;
  }
  vector<dir_ent_t> parentEntries(parentInode.size / sizeof(dir_ent_t) + 1);
  read(parentInodeNumber, parentEntries.data(), parentInode.size);
  bool entryAdded = false;
  for(auto& entry : parentEntries) {
    if(entry.inum == -1) {
      entryAdded = true;
      strncpy(entry.name, name.c_str(), sizeof(entry.name));
      entry.name[sizeof(entry.name) - 1] = '\0';
      entry.inum = newInodeNumber;
      break;
    }
  }
  if(!entryAdded) {
    parentEntries.at(parentInode.size / sizeof(dir_ent_t)).inum = newInodeNumber;
    strncpy(parentEntries.at(parentInode.size / sizeof(dir_ent_t)).name, name.c_str(), sizeof(dir_ent_t::name));
    parentInode.size += sizeof(dir_ent_t);
  }
  int numBlocks = (parentInode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  int offset = 0;
  for(int i = 0; i < numBlocks; i++) {
    disk->writeBlock(parentInode.direct[i], &parentEntries.at(offset));
    offset += UFS_BLOCK_SIZE / sizeof(dir_ent_t);
  }
  int parentInodeBlockNumber = super.inode_region_addr + (parentInodeNumber / (UFS_BLOCK_SIZE / sizeof(inode_t)));
  char parentBlock[UFS_BLOCK_SIZE];
  disk->readBlock(parentInodeBlockNumber, parentBlock);
  memcpy(parentBlock +(parentInodeNumber % (UFS_BLOCK_SIZE / sizeof(inode_t))) * sizeof(inode_t), &parentInode, sizeof(inode_t));
  disk->writeBlock(parentInodeBlockNumber, parentBlock);
  for(int i = 0; i < super.inode_bitmap_len; i++) {
    disk->writeBlock(super.inode_bitmap_addr + i, &inodeBitmap.at(i * UFS_BLOCK_SIZE));
  }
  for(int i = 0; i < super.data_bitmap_len; i++) {
    disk->writeBlock(super.data_bitmap_addr + i, &dataBitmap.at(i * UFS_BLOCK_SIZE));
  }
  return newInodeNumber;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  inode_t inode;
  if (stat(inodeNumber, &inode)) {
    return -EINVALIDINODE;
  }
  if (size > MAX_FILE_SIZE) {
    return -EINVALIDSIZE;
  }
  if (inode.type != UFS_REGULAR_FILE) {
    return -EINVALIDTYPE;
  }
  super_t super;
  readSuperBlock(&super);
  unsigned char dataBitmap[UFS_BLOCK_SIZE];
  readDataBitmap(&super, dataBitmap);
  int requiredBlocks = (size + (UFS_BLOCK_SIZE - 1)) / UFS_BLOCK_SIZE;
  requiredBlocks = min(requiredBlocks, (int)(sizeof(inode.direct) / sizeof(inode.direct[0])));
  vector<int> allocatedBlocks;
  for (int i = 0; i < super.num_data && allocatedBlocks.size() < (size_t)requiredBlocks; i++) {
    if (!(dataBitmap[i >> 3] & (1 << (i & 7)))) {
      allocatedBlocks.push_back(super.data_region_addr + i);
    }
  }
  char tempBlock[UFS_BLOCK_SIZE];
  int totalWritten = 0;
  int chunkSize;
  for (size_t i = 0; i < allocatedBlocks.size(); i++) {
    chunkSize = min((int)UFS_BLOCK_SIZE, size - totalWritten);
    memcpy(tempBlock, (char*)(buffer) + totalWritten, chunkSize);
    disk->writeBlock(allocatedBlocks.at(i), tempBlock);
    inode.direct[i] = allocatedBlocks.at(i);
    dataBitmap[(allocatedBlocks.at(i) - super.data_region_addr) >> 3] |= 1 << ((allocatedBlocks.at(i) - super.data_region_addr) & 7);
    totalWritten += chunkSize;
  }
  inode.size = totalWritten;
  int blockIdx = super.inode_region_addr + (inodeNumber / (UFS_BLOCK_SIZE / sizeof(inode_t)));
  int offset = (inodeNumber & ((UFS_BLOCK_SIZE / sizeof(inode_t)) - 1)) * sizeof(inode_t);
  disk->readBlock(blockIdx, tempBlock);
  memcpy(tempBlock + offset, &inode, sizeof(inode_t));
  disk->writeBlock(blockIdx, tempBlock);
  for (int i = 0; i < super.data_bitmap_len; i++) {
    disk->writeBlock(super.data_bitmap_addr + i, &dataBitmap[i * UFS_BLOCK_SIZE]);
  }
  return totalWritten;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  inode_t parentInode;
  if(stat(parentInodeNumber, &parentInode) != 0) {
    return -EINVALIDINODE;
  }
  if(parentInode.type != UFS_DIRECTORY) {
    return -EINVALIDTYPE;
  }
  int childInodeNumber = lookup(parentInodeNumber, name);
  if(childInodeNumber < 0) {
    return childInodeNumber;
  }
  inode_t childInode;
  if(stat(childInodeNumber, &childInode) != 0) {
    return -EINVALIDINODE;
  }
  if(childInode.type == UFS_DIRECTORY) {
    int numEntries = childInode.size / sizeof(dir_ent_t);
    if(numEntries != 2) {
      return -ENOTEMPTY;
    }
    dir_ent_t entries[2];
    if(read(childInodeNumber, entries, sizeof(entries)) != sizeof(entries)) {
      return -EINVALIDINODE;
    }
    if(strcmp(entries[0].name, ".") != 0 || entries[0].inum != childInodeNumber || strcmp(entries[1].name, "..") != 0 || entries[1].inum != parentInodeNumber) {
      return -ENOTEMPTY;
    }
  }
  bool found = false;
  char block[UFS_BLOCK_SIZE];
  int numBlocks = (parentInode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  for(int i = 0; i < numBlocks; i++) {
    if(parentInode.direct[i] == 0) {
      continue;
    }
    disk->readBlock(parentInode.direct[i], block);
    dir_ent_t *entries = (dir_ent_t*)block;
    int entriesPerBlock = UFS_BLOCK_SIZE / sizeof(dir_ent_t);
    for(int j = 0; j < entriesPerBlock; j++) {
      if(entries[j].inum == childInodeNumber && strcmp(entries[j].name, name.c_str()) == 0) {
        for(int k = j; k < entriesPerBlock - 1; k++) {
          entries[k] = entries[k + 1];
        }
        memset(&entries[entriesPerBlock - 1], 0, sizeof(dir_ent_t));
        parentInode.size -= sizeof(dir_ent_t);
        disk->writeBlock(parentInode.direct[i], block);
        super_t super;
        readSuperBlock(&super);
        int parentBlockNumber = super.inode_region_addr + (parentInodeNumber /(UFS_BLOCK_SIZE / sizeof(inode_t)));
        char parentInodeBlock[UFS_BLOCK_SIZE];
        disk->readBlock(parentBlockNumber, parentInodeBlock);
        inode_t *parent = (inode_t*)(parentInodeBlock + (parentInodeNumber % (UFS_BLOCK_SIZE / sizeof(inode_t))) * sizeof(inode_t));
        *parent = parentInode;
        disk->writeBlock(parentBlockNumber, parentInodeBlock);
        found = true;
        break;
      }
    }
    if(found) {
      break;
    }
  }
  if(!found) {
    return -ENOTFOUND;
  }
  super_t super;
  readSuperBlock(&super);
  unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inodeBitmap);
  inodeBitmap[childInodeNumber / 8] &= ~(1 << (childInodeNumber % 8));
  unsigned char dataBitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
  readDataBitmap(&super, dataBitmap);
  int numDataBlocks = (childInode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  for(int i = 0; i < numDataBlocks; i++) {
    if(childInode.direct[i] == 0) continue;
    int dataBlockIndex = childInode.direct[i] - super.data_region_addr;
    dataBitmap[dataBlockIndex / 8] &= ~(1 << (dataBlockIndex % 8));
  }
  for(int i = 0; i < super.inode_bitmap_len; i++) {
    disk->writeBlock(super.inode_bitmap_addr + i, &inodeBitmap[i * UFS_BLOCK_SIZE]);
  }
  for(int i = 0; i < super.data_bitmap_len; i++) {
    disk->writeBlock(super.data_bitmap_addr + i, &dataBitmap[i * UFS_BLOCK_SIZE]);
  }
  return 0;
}