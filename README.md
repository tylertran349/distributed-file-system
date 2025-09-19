# Distributed File System

A distributed file system implementation built from scratch using C++. This project implements a working distributed file server with a custom HTTP framework (`gunrock_web`) and provides a complete file system solution.

## Project Goals

This project was developed to explore and understand:

- On-disk structures for file systems
- File system internals and implementation details
- Distributed storage systems and their architecture
- HTTP/REST API design for file operations

## Architecture Overview

The project consists of three main components: [read-only file system utilities](#read-only-file-system-utilities) for inspecting on-disk storage, [read/write file system utilities](#readwrite-file-system-utilities) for modifying storage, and a [distributed file system](#distributed-file-system-1) that uses the local file system to provide network-accessible file operations. The implementation follows a layered approach where each component builds upon the previous one.

The project focuses on persistent storage across multiple abstraction layers. Each layer operates with different interfaces: the disk layer works with raw disk blocks, the local file system organizes these blocks into a hierarchical structure, and the API layer provides an object abstraction over the file system. Each component has its own unique physical reality and provides specific abstractions to the layers above.

## Distributed File System

### Background

The main idea behind a distributed file system is that multiple clients can access the same file system at the same time. One
popular distributed file system, which serves as inspiration for this
project, is Amazon's S3 storage system. S3 is used widely and provides
clear semantics with a simple REST/HTTP API for accessing data. With
these basics in place, S3 provides the storage layer that powers many
of the modern apps I use every day.

Like local file systems, distributed file systems support a number of
high level file system operations. This implementation provides
`read()`, `write()`, and `delete()` operations on objects using
HTTP APIs.

### HTTP/REST API

The distributed file system supports two different entity types:
`files` and `directories`. These entities are accessed using
standard HTTP/REST network calls. All URL paths begin with `/ds3/`,
which defines the root of the file system.

Files are created or updated using the HTTP `PUT` method where the URL
defines the file name and path and the body of the PUT request
contains the entire contents of the file. If the file already exists,
the PUT call overwrites the contents with the new data sent via the
body.

In the system, directories are created implicitly. If a client PUTs a
file located at `/ds3/a/b/c.txt` and directories `a` and `b` do not
already exist, they will be created as part of handling the
request. If one of the directories on the path exists as a file
already, like `/a/b`, then it is an error.

Files are read using the HTTP `GET` method, specifying the file
location as the path of the URL and the server returns the
contents of the file as the body in the response. Directories are also read
using the HTTP `GET` method, but directories list the entries
in a directory. Directory entries are encoded by putting each directory
entry on a new line and regular files are listed directly, and
directories are listed with a trailing "/". For `GET` on a directory
the entries for `.` and `..` are omitted. For example, `GET` on `/ds3/a/b` will
return:

`c.txt`

And `GET` on `/ds3/a/` will return:

`b/`

The listed entries should be sorted using standard string comparison
sorting functions.

Files are deleted using the HTTP `DELETE` method, specifying the
file location as the path of the URL. Directories are also deleted
using `DELETE` but deleting a directory that is not empty
is an error.

The API handlers are implemented in
[DistributedFileSystemService.cpp](gunrock_web/DistributedFileSystemService.cpp).

Since Gunrock is a HTTP server, command line utilities
like cURL can be used to test it. Here are a few example cURL commands:

```bash
% curl -X PUT -d "file contents" http://localhost:8080/ds3/a/b/c.txt
% curl http://localhost:8080/ds3/a/b/c.txt
file contents
% curl http://localhost:8080/ds3/a/b/
c.txt
% curl http://localhost:8080/ds3/a/b
c.txt
% curl http://localhost:8080/ds3/a
b/
% curl -X DELETE http://localhost:8080/ds3/a/b/c.txt
% curl http://localhost:8080/ds3/a/b/
%
```

### Dealing with errors

The distributed storage
interface uses a sequence of LocalFileSystem calls. Although
each of these calls individually ensures that they will not modify
the disk when they have an error, since the implementation uses
several LocalFileSystem calls it needs to clean up when something goes
wrong. The key principle is that if an API call has an error, there
should not be any changes to the underlying disk or local file system.

The LocalFileSystem is cleaned up on errors using the
[Disk](gunrock_web/include/Disk.h) interface for transactions. When using
transactions, if a file system call can change the
disk's state, it starts a transaction by calling
`beginTransaction`. As the implementation for an API call proceeds,
if the call is successful then it can `commit` the transaction to
ensure that all file system modifications persist. If the call ends
with an error, it can call `rollback` to reverse any writes that
happened before the error.

There are four types of errors that the distributed file system can
return. First, `ClientError::notFound()` for any API calls that
try to access a resource that does not exist. Second, `ClientError::insufficientStorage()` for operations that modify the
file system and need to allocate new blocks but the disk does not have
enough storage to satisfy them. Third, `ClientError::conflict()`
if an API call tries to create a directory in a location where a file
already exists. Fourth, `ClientError::badRequest()` for all other
errors.

To return an error to the client, in the
`DistributedFileSystemService.cpp` file, the implementation throws an exception using the
[ClientError](gunrock_web/include/ClientError.h) exception class, and
the gunrock web framework catches these errors and converts them to
the right HTTP response and status code for that error.

## Local file system

### On-Disk File System: A Basic Unix File System

The on-disk file system structures follow that of the
very simple file system discussed
[here](https://pages.cs.wisc.edu/~remzi/OSTEP/file-implementation.pdf). On-disk,
the structures are as follows:

- A single block (4KB) super block
- An inode bitmap (can be one or more 4KB blocks, depending on the number of inodes)
- A data bitmap (can be one or more 4KB blocks, depending on the number of data blocks)
- The inode table (a multiple of 4KB-sized blocks, depending on the number of inodes)
- The data region (some number of 4KB blocks, depending on the number of data blocks)

More details about on-disk structures can be found in the header
[ufs.h](ufs.h). Specifically, this defines a very
specific format for the super block, inode, and directory
entries. Bitmaps have one bit per allocated unit as described in
the referenced documentation.

As for directories, here is a little more detail. Each directory has
an inode, and points to one or more data blocks that contain directory
entries. Each directory entry should be simple, and consist of 32
bytes: a name and an inode number pair. The name should be a
fixed-length field of size 28 bytes; the inode number is just an
integer (4 bytes). When a directory is created, it should contain two
entries: the name `.` (dot), which refers to this new directory's
inode number, and `..` (dot-dot), which refers to the parent
directory's inode number. For the root directory in a file system,
both `.` and `..` refer to the root directory.

When the server is started, it is passed the name of the file system
image file. The image is created by a tool called `mkfs`.
It is self-explanatory and can be found
[here](gunrock_web/mkfs.c).

When accessing the files on an image, the server reads in the
superblock, bitmaps, and inode table from disk as needed. When writing
to the image, it updates these on-disk structures accordingly.

One important aspect of the on-disk structure is that the system needs to
assume that the server can crash at any time, so all disk writes need
to leave the file system in a consistent state. To maintain
consistency typically you'd need to order writes carefully. For
this implementation, the system ensures that the disk is
in a consistent state after each `LocalFileSystem` call returns.

The file-system on-disk format cannot be changed.

For more detailed documentation on the local file system specification,
see [LocalFileSystem.h](gunrock_web/include/LocalFileSystem.h)
and the implementation [LocalFileSystem.cpp](gunrock_web/LocalFileSystem.cpp). Also,
see [Disk.h](gunrock_web/include/Disk.h) for the interface for accessing
the disk.

### Bitmaps for block allocation

I use on-disk bitmaps to keep track of entries (inodes and data blocks) that
the file system has allocated. For my bitmaps for each byte, the least
significant bit (LSB) is considered the first bit, and the most significant
bit (MSB) is considered the last bit. So if the first bit of a two byte
bitmap is set, it will look like this in hex:

```
byte position:  0  1
hex value:     01 00

bit position   0                               15
bit value:     1 0 0 0  0 0 0 0  0 0 0 0  0 0 0 0
```

and if the last bit is set it will look like this in hex:

```
byte position:  0  1
hex value:     00 80

bit position   0                               15
bit value:     0 0 0 0  0 0 0 0  0 0 0 0  0 0 0 1
```

### LocalFileSystem `write` and `read` semantics

In my file system, I don't have a notion of appending or modifying data.
Conceptually, when I get a `write` call I overwrite the entire contents
of the file with the new contents of the file, and write calls specify
the complete contents of the file.

Calls to `write`, `create`, and `unlink` reuse existing data
blocks. If the new file uses fewer data blocks, the system frees the
extra data blocks. If the new file needs more data blocks, the system
adds these new blocks while still reusing the current blocks for the
first part of the data.

Calls to `read` always read data starting from the beginning of the file,
but if the caller specifies a size of less than the size of the object
then only these bytes are returned. If the caller specifies a size
of larger than the size of the object, then only the bytes
in the object are returned.

### Allocating data blocks and inodes

When allocating a new data block or inode, the system always
uses the lowest numbered entry that is available.

### LocalFileSystem out of storage errors

One important class of errors that the `LocalFileSystem` handles is
out of storage errors. Out of storage errors can happen when one of the
file system modification calls -- `create` and `write` -- does not
have enough available storage to complete the request.

For `write` calls, the system writes as many bytes as possible and
returns success to the caller with the number of bytes actually
written.

For `create` calls, the system allocates both an inode and a disk
block for directories. If one of these entities is allocated but
the other cannot be allocated, the system frees allocated inodes or disk
blocks before returning an error from the call.

## Read-only file system utilities

For debugging disk images, the project includes seven small
command-line utilities that access a given disk image. Example disk images and expected outputs are included in the
[disk_testing](gunrock_web/tests/disk_images) directory. The utilities can handle multiple different disk image configurations
and contents.

The `ds3ls`, `ds3bits`, and `ds3cat` utilities use
the "read" calls from the local file system. These read
calls are `lookup`, `stat`, and `read`. The `ds3bits` utility includes
`readSuperBlock`, `readInodeBitmap`, and
`readDataBitmap` functions.

The project includes a full set of test cases for testing the
read-only parts of the local file system. They can be run
using the `test-readonly.sh` testing script.

Note: The utilities are tested on correct disk images -- all data on disk in the test cases is consistent and
correct.

### Error handling

In general, most of the error handling is done by the
`LocalFileSystem.cpp` implementation. These utilities call the
underlying file system methods and convey an error when an error
occurs, where the actual error checking logic happens in the file
system implementation. For all utilities, there is a single
string that is written to standard error for all errors that may
be encountered.

### The `ds3ls` utility

The `ds3ls` prints the contents of a directory. This utility takes two
arguments: the name of the disk image file to use and the path of the
directory or file to list within the disk image. For directories, it
prints all of the entries in the directory, sorted using the
`std::sort` function. Each entry goes on its own line. For files, it
prints just the information for that file. Each entry will include the
inode number, a tab, the name of the entry, and finishing it off with
a newline.

For all errors, a single error string is used: `Directory not found`
printed to standard error (e.g., cerr), and the process exits
with a return code of 1. On success, the program's return code is 0.

### The `ds3cat` utility

The `ds3cat` utility prints the contents of a file to standard
output. It takes the name of the disk image file and an inode number
as the only arguments. It prints the contents of the file that is
specified by the inode number.

For this utility, first the string `File blocks` is printed with a newline
at the end and then each of the disk block numbers for the file
is printed to standard out, one disk block number per line. After printing the
file blocks, an empty line with only a newline is printed.

Next, the string `File data` is printed with a newline at the end and then
the full contents of the file are printed to standard out. There is no need
to differentiate between files and directories, for this utility
everything is considered to be data and is printed to standard
out.

Calling `ds3cat` on a directory is an error. For all errors the
string `Error reading file` is printed to standard error and the process
return code is set to 1.

### The `ds3bits` utility

The `ds3bits` utility prints metadata for the file system on a disk
image. It takes a single command line argument: the name of a disk
image file.

It prints metadata about the file system starting with the
string `Super` on a line, then `inode_region_addr` followed by a space
and the inode region address from the super block. Next, it
prints the string `inode_region_len` followed by a space and the
value from the super block. Finally, it prints the string
`num_inodes` followed by a space and the value from the super
block. This process is repeated for the data region.

After printing the super block contents, an empty line is printed.

Next it prints the inode and data bitmaps. Each bitmap starts with a
string on its own line, `Inode bitmap` and `Data bitmap`
respectively. For each bitmap, the byte value is printed, formatted as an
`unsigned int` followed by a space. Each byte is followed by a space, including the last byte, and
after printing all of the bytes, a single newline
character is printed.

The inode bitmap is printed first, followed by a blank line consisting of
only a single newline character, then the data bitmap.

## Read/write file system utilities

In addition to the utilities used to read a file system
image, the project also includes four simple utilities to modify disk images
using the `LocalFileSystem.cpp` implementation.

Note: On success, none of these utilities print anything to standard
out, but they use a process return code of 0 to signify success.

The read-only utilities are used to generate the outputs for testing the read/write
utilities, so that part of the
`LocalFileSystem.cpp` implementation must work correctly.

### The `ds3mkdir` and `ds3touch` utilities

The `ds3mkdir` and `ds3touch` utilities create directories and files
respectively in the file system. On the command line, these utilities
take the disk image file, the parentInode for the directory where the
new entry will be created, and the name of the new entry.

For all errors, the string `Error creating directory`
(`ds3mkdir`) or `Error creating file` (`ds3touch`) is printed to standard error
and the program exits with a return code of 1. As with the underlying
`LocalFileSystem.cpp` implementation, creating a directory or file
that already exists is not an error as long as the type (file or
directory) is consistent with the existing entity.

### The `ds3cp` utility

The `ds3cp` utility copies a file from the computer into the disk
image using the `LocalFileSystem.cpp` implementation. It takes three
command line arguments, the disk image file, the source file from the
computer that needs to be copied in, and the inode for the destination
file within the disk image.

For all errors, the string `Could not write to dst_file` is printed to
standard error and the program exits with return code 1. Files
that exist on the computer are used for testing, so if there is an error
opening or reading from the source file, any error
message that makes sense can be printed.

### The `ds3rm` utility

The `ds3rm` utility removes a file or empty directory from the disk
image's file system. It takes three arguments: the disk image file
name, the inode for the parent directory, and the name of the file or
directory that needs to be deleted. For all errors, the string
`Error removing entry` is printed to standard error and the program exits with return code 1.

## Building and Running

The project includes the following main source files: `LocalFileSystem.cpp`,
`DistributedFileSystemService.cpp`, `ds3ls.cpp`, `ds3cat.cpp`,
`ds3bits.cpp`, `ds3mkdir.cpp`, `ds3touch.cpp`, `ds3cp.cpp`, and
`ds3rm.cpp`. All files are required to build and run the complete system.
