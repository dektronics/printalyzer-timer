#!/usr/bin/perl

##
## This script performs CRC-32 checksum calculations on an STM32
## binary image, and writes that checksum to a file suitable
## for appending to that binary.
##

use 5.006;
use strict;
use warnings;

#
# Calculate the CRC-32 checksum on a block of data, using the same algorithm
# used by the hardware CRC module inside the STM32F4 microcontroller.
#
# Based on the C implementation posted here:
# https://community.st.com/s/question/0D50X0000AIeYIb/stm32f4-crc32-algorithm-headache
#
sub stm_crc32_fast {
    my ($crc, $data) = @_;
    my @crc_table = (0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
                     0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
                     0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61,
                     0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD);

    $crc = ($crc ^ $data) & 0xFFFFFFFF;
    $crc = (($crc << 4) & 0xFFFFFFFF) ^ $crc_table[($crc >> 28) & 0xFF];
    $crc = (($crc << 4) & 0xFFFFFFFF) ^ $crc_table[($crc >> 28) & 0xFF];
    $crc = (($crc << 4) & 0xFFFFFFFF) ^ $crc_table[($crc >> 28) & 0xFF];
    $crc = (($crc << 4) & 0xFFFFFFFF) ^ $crc_table[($crc >> 28) & 0xFF];
    $crc = (($crc << 4) & 0xFFFFFFFF) ^ $crc_table[($crc >> 28) & 0xFF];
    $crc = (($crc << 4) & 0xFFFFFFFF) ^ $crc_table[($crc >> 28) & 0xFF];
    $crc = (($crc << 4) & 0xFFFFFFFF) ^ $crc_table[($crc >> 28) & 0xFF];
    $crc = (($crc << 4) & 0xFFFFFFFF) ^ $crc_table[($crc >> 28) & 0xFF];

    return $crc;
}

#
# Iterate across an entire binary string and compute the CRC-32 checksum.
#
sub stm_crc32_fast_block {
    my ($crc, $input) = @_;

    foreach my $x (unpack ('V*', $input)) {
        $crc = stm_crc32_fast($crc, $x);
    }

    return $crc;
}

# Collect the command line arguments
my ($infile, $outfile) = @ARGV;
die "Usage: $0 INFILE OUTFILE\n" if not $outfile;

# Open the input file as binary
open my $in, '<', $infile or die;
binmode $in;

# Read the input file
my $cont = '';
while (1) {
    my $success = read $in, $cont, 1024, length($cont);
    die $! if not defined $success;
    last if not $success;
}
close $in;

# Pad the file out to 384K, with room for the checksum
my $remaining = (0x60000 - length($cont)) - 4;
if ($remaining > 0) {
    $cont .= "\xFF" x $remaining;
}

# Calculate the CRC32 checksum
my $crc = pack('V', stm_crc32_fast_block(0xFFFFFFFF, $cont));

# Write the output file
open my $out, '>', $outfile or die;
print $out $crc;
close $out;
