# How to do the labs

A quick guide on how to approach the labs.

## What do I need to do?

In the CSE 451 labs your goal is to implement additional functionality in `xk` that meets the spec.
More concretely we assume your implementation meets the spec if it passes the lab tests.

At the risk of being redudctive, in CSE 451 all you *really* need to do is submit an implementation which passes the tests.

## How do I know what I need to do?

The lab specs describe what your implementation needs to do and how to test it.
At a minimum you should read each lab spec to know what you need to implement.

For lab specs with separate top-level parts, it will usually be possible to complete the implementation of a single part without referencing later parts. However, offten context given further on in the spec can help you understand earlier parts.

## How do I do?

You should/will have a design doc for each lab. For lab 1 its provided, and for labs beyond that we require you to make one. Once you've thought through the implementation usually you'll probably have a good idea of where to start. If you're missing one though: you can work backwards from what you need to meet the spec. For example: for a lab spec that requires new syscalls, start by drafting the system call.

A key mindset you should have when implementing though is that **the repo is your oyster**. You should feel free to edit any file you need to to make your OS work (i.e.: to make the tests pass). However there is one caveat: when submitting there's a list of files which the autograder overwrites when testing your code (in order to ensure students don't cheat the tests). This means any changes made to these files will be lost when submitting:
- Everything in `user/`
- `inc/param.h`, `inc/test.h`
- `kernel/Makefrag`, `kernel/initcode.S`
- `Makefile`, `crash_safety_test.py`, `sign.pl`

Sometimes modifying these files (typically the tests in `user/`) can be very helpful for debugging. In these cases you should feel free to go at it (just note that none of the logic your OS needs to work that you add should be in these files).

As long as you use `git` to track good states of your repo, it's good to explore designs with the understanding that any file is free to be edited.

## How do I know I'm done?

Run the tests. If they all pass, follow the submission instructions to submit online. If they all pass on Gradescope then you're truly done!

There are no hidden tests! If something fails on Gradescope it usually has to do with Gradescope running your program with different parameters.
