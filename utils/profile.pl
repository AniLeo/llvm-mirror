#!/usr/bin/perl -w
#
# Program:  profile.pl
#
# Synopsis: Insert instrumentation code into a program, run it with the JIT,
#           then print out a profile report.
#
# Syntax:   profile.pl [OPTIONS] bytecodefile <arguments>
#
# OPTIONS may include one or more of the following:
#     -block - Enable basic block level profiling
#

my $ProfilePass = "-insert-function-profiling";

# Parse arguments...
while (scalar(@ARGV) and ($_ = $ARGV[0], /^[-+]/)) {
  shift;
  last if /^--$/;  # Stop processing arguments on --

  # List command line options here...
  if (/^-block$/) { $ProfilePass = "-insert-block-profiling"; next; }

  print "Unknown option: $_ : ignoring!\n";
}

die "Must specify LLVM bytecode file as first argument!" if (@ARGV == 0);

my $BytecodeFile = $ARGV[0];

shift @ARGV;

my $LLIPath = `which lli`;
$LLIPath = `dirname $LLIPath`;
chomp $LLIPath;

my $LibProfPath = $LLIPath . "/../../lib/Debug/libprofile_rt.so";

system "opt $ProfilePass < $BytecodeFile | lli -fake-argv0 '$BytecodeFile'" .
       " -load $LibProfPath - " . (join ' ', @ARGV);

system "llvm-prof $BytecodeFile";
