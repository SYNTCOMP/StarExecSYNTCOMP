# Reproducing the rankings and graphs
Most of the work is done using the `inforeal.py` Python script. Below, we
mention how to run the script for realizability tracks.

## LTL
The following can be executed to obtain the graphs from the StarExec
information and output files for the corresponding job. We start with
realizability.

### Realizability

1. We clean the dataset because of acacia outputing a verdict when it is
   actually crashing. Concretely:
   `grep -r "std::bad_alloc" ~/Downloads/ltl-real-output/LTL/abonsai___real -l | sed "s/^.*\/\(.*\)\.txt$/\1/g" > excludes.txt`
   and
   `grep -r "Too many acc" ~/Downloads/ltl-real-output/LTL/abonsai___real -l | sed "s/^.*\/\(.*\)\.txt$/\1/g" >> excludes.txt`
2. We run the python script to generate the graphs and get the summary of
   results: 
   `python3 inforeal.py LTLreal ~/Downloads/ltl-real/Job59530_info.csv --timeout 1800 --expairs 612435292 612435294 612429172 612429173 612429174 612429175 612431137 612431139 $(cat excludes.txt)`

Note the list jobs excluded above. Those are instances of SPORE
having a memory out that manifests itself in the form of a
`killDeadlockedJobPair` status in StarExec. The tool then panics and outputs a
wrong verdict.

### Synthesis
We add `--synthesis` as an option to the script.

## Parity games
In this case it suffices to use the script as follows

`python3 inforeal.py PGreal file.csv --timeout 1800`

and for synthesis

`python3 inforeal.py PGsynt file.csv --timeout 1800 --synthesis`

## LTLF realizability
Here again the script works but we have to exclude a number of pair ids
corresponding to the tool lisa-syntcomp having a memory out like that of SPORE
above, again getting a `killDeadlockedJobPair`.

The command is `python3 inforeal.py LTLFreal ~/Downloads/ltlf-real/Job59546_info.csv --expairs $(cat excludes.txt)` with
the contents of `excludes.txt` in this case being the following.

612466157
612466158
612466159
612463357
612463358
612463359
612468669
612468670
612468671
612463965
612463966
612463967
612464845
612464846
612464847
612468317
612468318
612468319
612465389
612465390
612465391
612467469
612467470
612467471
612469613
612469614
612469615
612465661
612465662
612465663
612466829
612466830
612466831
612463981
612463982
612463983
612464397
612464398
612464399
612465245
612465246
612465247
612467645
612467646
612467647
612469805
612469806
612469807
612463149
612463150
612463151
612463725
612463726
612463727
612469309
612469310
612469311
612468269
612468270
612468271
612470109
612470110
612470111
612466669
612466670
612466671
612464653
612464654
612464655
612467485
612467486
612467487
612465116
612469868
612467084
612466140
612466444
