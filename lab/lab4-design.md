# Lab 4 Design Doc: File System

## Instruction (Please Remove Upon Submission)
Follow the guidelines in [designdoc.md](designdoc.md).

This is a template for lab 4 design doc, you are welcomed to make adjustments, but  **you must answer the provided design questions**.

Once your design doc is finished, you should **submit it on Gradescope as a pdf**.
You can generate a pdf of this markdown file by running
```
pandoc -f markdown -o lab4-dd.pdf lab/lab4-design.md
```

Please include your names in the design doc you submit (helps us if you forget to tag partner on gradescope).

**Names:**

## Overview

[ your text here ]

### Major Parts
> For each part below, explain the goals and key challenges to accomplishing these goals.
> Please also explain how different parts of this lab interact with each other.

Part A: File Writes

  - [ your text here ]

Part A: File Creation

  - [ your text here ]

Part A: File Deletion

  - [ your text here ]

Part B: File Synchronization

  - [ your text here ]

Part C: Crash Safety

  - [ your text here ]

Interactions 

  - [ your text here ]


<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->

## In-depth Analysis and Implementation

### Part A: File Writes

**Functions To Implement & Modify**

> This section should describe the behavior and implementation details of the functions 
in sufficient detail that another student/TA would be convinced of its correctness. Include 
any data structures accessed and structs modified and specify the type of locks used and which critical 
sections you need to guard. If any of the Design Questions lead to important implementation details, you must
also specify them here.

[ your text here ]

**Design Questions**

> What data layout is the xk file system currently using?

- [ your answer here ]

> What data layout are you using for supporting file extension? What changes are you making to the on disk inode? What size is your updated on disk inode?

- [ your answer here ]

> How does mkfs.c set up inodes for the existing files (files unders user/*)? How does mkfs.c calculate the number of data blocks needed for each file?

- [ your answer here ]

> What changes do you need to make to mkfs.c to reflect your new data layout?

- [ your answer here ]

> For an overwrite (no changes to file size, existing data is overwritten), what structures must be updated and how are they updated? 
> Think about data blocks, the bitmap, in memory and on disk inode.

- [ your answer here ]

> Does an append (write past current end of file) always result in the allocation of new data blocks? Please explain.

- [ your answer here ]

> For an append (changes to file size and data), what structures must be updated and how are they updated? 
> Think about data blocks, the bitmap, in memory and on disk inode.

- [ your answer here ]

What does the valid field mean in the in-memory inode? When is the valid field set to true?

- [ your answer here ]

> Upon an append, what happens if you only update the on disk inode and not the in-memory inode? 
> Will a subsequent read be able to read the new data? Will the file retain the write after a power cycle? Please explain.

- [ your answer here ]

> Upon an append, what happens if you only update the in-memory inode and not the on disk inode? 
> Will a subsequent read be able to read the new data? Will the file retain the write after a power cycle? Please explain.

- [ your answer here ]

> Where does the on disk inode live? How can we update the on disk inode? Hint: take a look at `read_dinode` and see if you can come up with a corresponding `write_dinode`.

- [ your answer here ]

> For file extension, you may need to allocate new data blocks, what can you use to achieve this?

- [ your answer here ]


**Corner Cases**
> Describe any special/edge cases and how your program logic handles these cases.

[ your text here ]

**Test Plans**

> List the specific behaviors/cases that the provided tests cover (along with any
other important cases that you might think of that the tests don't cover) **and** 
justify how and why your program logic will correctly handle the tested behaviors (preferably by citing
specific aspects of your implementation described in the "Functions to Implement & Modify" section).

[ your text here ]

<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->

### Part A: File Creation

**Functions To Implement & Modify**

> This section should describe the behavior and implementation details of the functions 
in sufficient detail that another student/TA would be convinced of its correctness. Include 
any data structures accessed and structs modified and specify the type of locks used and which critical 
sections you need to guard. If any of the Design Questions lead to important implementation details, you must
also specify them here.

[ your text here ]

**Design Questions**

> How do you know if a file needs to be created? What conditions must be met for file creation?

- [ your answer here ]

> Assuming no file deletion, creating a new file should always create a new inode. How can you create a new on disk inode?

- [ your answer here ]

> Assuming no file deletion, creating a new file should always cause the parent directory to extend. How do you extend the parent directory? 

- [ your answer here ]

> How do you map an offset of the inode table to an inode number and vice versa?

- [ your answer here ]

**Corner Cases**
> Describe any special/edge cases and how your program logic handles these cases.

[ your text here ]

**Test Plans**

> List the specific behaviors/cases that the provided tests cover (along with any
other important cases that you might think of that the tests don't cover) **and** 
justify how and why your program logic will correctly handle the tested behaviors (preferably by citing
specific aspects of your implementation described in the "Functions to Implement & Modify" section).

[ your text here ]

<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->

### Part A: File Deletion

**Functions To Implement & Modify**

> This section should describe the behavior and implementation details of the functions 
in sufficient detail that another student/TA would be convinced of its correctness. Include 
any data structures accessed and structs modified and specify the type of locks used and which critical 
sections you need to guard. If any of the Design Questions lead to important implementation details, you must
also specify them here.

[ your text here ]

**Design Questions**

> You can only unlink files that do not have open references. How do you check whether there are open references of given the file name?

- [ your answer here ]

> With file deletion, how are you updating the deleted inode so that it may be reused by a create? 

- [ your answer here ]

> With file deletion, how are you updating the deleted file's directory entry so that it may be reused by a create?

- [ your answer here ]

**Corner Cases**
> Describe any special/edge cases and how your program logic handles these cases.

[ your text here ]

**Test Plans**

> List the specific behaviors/cases that the provided tests cover (along with any
other important cases that you might think of that the tests don't cover) **and** 
justify how and why your program logic will correctly handle the tested behaviors (preferably by citing
specific aspects of your implementation described in the "Functions to Implement & Modify" section).

[ your text here ]

<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->

### Part B: File Synchronization

**Functions To Implement & Modify**

> This section should describe the behavior and implementation details of the functions 
in sufficient detail that another student/TA would be convinced of its correctness. Include 
any data structures accessed and structs modified and specify the type of locks used and which critical 
sections you need to guard. If any of the Design Questions lead to important implementation details, you must
also specify them here.

[ your text here ]

**Design Questions**

> Please explain how you are protecting concurrent dup operations on the same inode?

- [ your answer here ]

> Please explain how you are protecting concurrent write operations on the same inode?

- [ your answer here ]

> Please explain how you are protecting concurrent read operations on the same inode? Do reads need to be protected? Why or why not?

- [ your answer here ]

> Please explain how you are protecting concurrent delete operations on the same file? Only one delete should succeed.

- [ your answer here ]

> Please explain how you are protecting concurrent create operations on the same file? Only one create should succeed.

- [ your answer here ]

**Corner Cases**
> Describe any special/edge cases and how your program logic handles these cases.

[ your text here ]

**Test Plans**

> List the specific behaviors/cases that the provided tests cover (along with any
other important cases that you might think of that the tests don't cover) **and** 
justify how and why your program logic will correctly handle the tested behaviors (preferably by citing
specific aspects of your implementation described in the "Functions to Implement & Modify" section).

[ your text here ]

<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->

### Part C: Crash Safety

**Functions To Implement & Modify**

> This section should describe the behavior and implementation details of the functions 
in sufficient detail that another student/TA would be convinced of its correctness. Include 
any data structures accessed and structs modified and specify the type of locks used and which critical 
sections you need to guard. If any of the Design Questions lead to important implementation details, you must
also specify them here.

[ your text here ]

**Design Questions**

> What crash-safe mechanism are you planning to implement and what changes do you need to make in mkfs.c to support your design?

- [ your answer here ]

> How does your design enable atomic update of multiple blocks (hence crash safety)?

- [ your answer here ]

> After a crash and upon a reboot, what recovery procedure are you applying to ensure the consistency of the file system?

- [ your answer here ]

> On boot, file system initializes itself via `iinit`. Assuming you are applying some recovery procedure, where in `iinit` should you run your recovery procedure?
> Should you do it before reading in the super block or after? Should you do it before reading in the inode table or after?

- [ your answer here ]

> If there are multiple file system operations happening concurrently, how does that affect your crash-safe mechanism? Is your crash-safe mechanism protected against concurrent accesses?

- [ your answer here ]

> What file system operations need to be protected against crashes? Does a read need to be crash-safe?

- [ your answer here ]


**Corner Cases**
> Describe any special/edge cases and how your program logic handles these cases.

[ your text here ]


**Test Plans**

> List the specific behaviors/cases that the provided tests cover (along with any
other important cases that you might think of that the tests don't cover) **and** 
justify how and why your program logic will correctly handle the tested behaviors (preferably by citing
specific aspects of your implementation described in the "Functions to Implement & Modify" section).

[ your text here ]

<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->


## Risk Analysis

### Unanswered Questions

[ your text here ]

### Staging of Work

[ your text here ]

### Time Estimation

[ your text here ]
