#!/usr/bin/perl

use strict;
use warnings;

use Getopt::Long;

# Collect the command line arguments
my $size_command = "size";
GetOptions('size=s' => \$size_command);

my $elf_file = $ARGV[0] or die "Missing ELF file parameter\n";

# Run "size" two ways to collect all the information
my $size_output_sum = `"$size_command" -B -d "$elf_file"`;
my $size_output_sec = `"$size_command" -A -d "$elf_file"`;

# Parse the total FLASH size out of the Berkeley format
my @sizes = $size_output_sum =~ /^\s*(\d+)\s+(\d+)\s+/m;

if (scalar(@sizes) != 2) {
    die "Malformed size output\n";
}

my $flash_size = int($sizes[0]) + int($sizes[1]);

# Parse the individual section sizes out of the SysV format
my %sections;
foreach my $line (split /^/m, $size_output_sec) {
    my @elements = $line =~ /^\.(\S+)\s+(\d+)\s+\d+$/m;
    if (scalar(@elements) == 2) {
        $sections{$elements[0]} = $elements[1];
    }
}

# Print the results
printf("Free space: %d (%d KB)\n", $sections{'end_fill'}, $sections{'end_fill'} / 1024);
printf("Total size: %d (%d KB)\n", $flash_size, $flash_size / 1024);
