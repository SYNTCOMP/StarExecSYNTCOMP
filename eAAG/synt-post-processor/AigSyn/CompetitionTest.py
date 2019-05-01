#!/usr/bin/env python

from AigerParser import *
from PyCuddBDD import *
from CompetitionTrnsSys import *
import sys
import os.path


location = "/home/mohamad/MSakr/AigSyn/tests/winning/"

if len(sys.argv) == 3 :
    winingRegion = sys.argv[2]
    fileName = sys.argv[1]
else:
    winingRegion = "cnt4n.aag-wregion.aag"
    fileName = "cnt4n.aag-result.aag"
    winingRegion = location + winingRegion
    fileName = location + fileName

if not os.path.isfile(winingRegion):
    print "False"
    sys.exit(winingRegion + ' cannot be found!')


if not os.path.isfile(fileName):
    print "False"
    sys.exit(fileName + ' cannot be found!')

_bdd = PyCuddBDD()

TrnsSys = None

try:
    resultParser = AigerFileParser(fileName)
    aig = resultParser.parse()
    TrnsSys = CompetitionTrnsSys(aig, _bdd)
except Exception as ex:
    print 'False'
    sys.exit('An Error happenned, it could be that ' + fileName + ' is not in a correct fromat')



#mcResult = sys.ModelCheck()

try:
    winRegParser = AigerFileParser(winingRegion)
    winRegAig = winRegParser.parse()
    result =  TrnsSys.ValidateWinningRegion(winRegAig).split(":")
except Exception as ex:
    print 'False'
    sys.exit('An Error happenned, it could be that ' + winingRegion + ' is not in a correct fromat')



print result[0]

if len(result) > 1:
    print result[1]

