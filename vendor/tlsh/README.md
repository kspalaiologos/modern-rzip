
# TLSH - Trend Micro Locality Sensitive Hash

TLSH is a fuzzy matching library.
Given a byte stream with a minimum length of 50 bytes
TLSH generates a hash value which can be used for similarity comparisons.
Similar objects will have similar hash values which allows for
the detection of similar objects by comparing their hash values.  Note that
the byte stream should have a sufficient amount of complexity.  For example,
a byte stream of identical bytes will not generate a hash value.

## What's New in TLSH 4.10.x
22/09/2021

Release version 4.8.x	- merged in pull requests for more stable installation.

Release version 4.9.x	- added -thread and -private options.
- Both versions are faster than previous versions, but they set the checksum to 00.
- This loses a very small part of the functionality.
- See 4.9.3 in the Change_History to see timing comparisons.

Release version 4.10.x	- a Python clustering tool.
- See the directory tlshCluster.

2020
- adopted by [Virus Total](https://developers.virustotal.com/v3.0/reference#files-tlsh)
- adopted by [Malware Bazaar](https://bazaar.abuse.ch/api/#tlsh)

TLSH has gained some traction. It has been included in STIX 2.1 and been ported to a number of langauges.

We have added a version identifier ("T1") to the start of the digest so that we can
cleary distinguish between different variants of the digest (such as non-standard choices of 3 byte checksum).
This means that we do not rely on the length of the hex string to determine if a hex string is a TLSH digest
(this is a brittle method for identifying TLSH digests).
We are doing this to enable compatibility, especially backwards compatibility of the TLSH approach.

The code is backwards compatible, it can still read and interpret 70 hex character strings as TLSH digests.
And data sets can include mixes of the old and new digests.
If you need old style TLSH digests to be outputted, then use the command line option '-old'

## Dedication

Thanks to Chun Cheng, who was a humble and talented engineer.

## Minimum byte stream length

The program in default mode requires an input byte stream with a minimum length of 50 bytes
(and a minimum amount of randomness - see note in Python extension below).

For consistency with older versions, there is a -conservative option which enforces a 256 byte limit.
See notes for version 3.17.0 of TLSH

## Computed hash

The computed hash is 35 bytes of data (output as 'T1' followed 70 hexidecimal characters. Total length 72 characters).
The 'T1' has been added as a version number for the hash - so that we can adapt the algorithm and still maintain
backwards compatibility.
To get the old style 70 hex hashes, use the -old command line option.

Bytes 3,4,5 are used to capture the information about the file as a whole
(length, ...), while the last 32 bytes are used to capture information about
incremental parts of the file.
