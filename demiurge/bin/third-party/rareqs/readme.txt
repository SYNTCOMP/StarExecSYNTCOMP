USAGE: rareqs [OPTIONS] filename
  'v': increase verbosity 
  'i': use incremental solver 
  'u': use unit propagation 
  'h l': start using incremental solver for levels <= l  
  'b': use blocking
  'p': use pure literal rule
  'r': use universal reduction
NOTES:
  If filename is '-', formula is read from the standard input.
  Options are given in in the standard getopt format. 

RECOMMENDED OPTIONS:
 -uupbh3
