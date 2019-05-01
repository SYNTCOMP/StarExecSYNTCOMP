#!/usr/bin/env python
from AigerParser import *
import time
import copy

#note that in this transition system we do not create a new variable
#as we are just intrested in strategies and winning region
#and the strategy contains no controllable inputs
class CompetitionTrnsSys(object):

    def __init__(self, aiger, bdd):
        #the number of variables is the number of inputs and latches,
        # *2 in order to create primed variables
        # bdd *2 in order to access var by index
        #last + 1 for output bdd with inputs
        self.varsLength = (((aiger.getMaxVarIndex() * 2) + 2) + 1)
        self._blackNb = 19
        self._vars = [None] * self.varsLength
        self._trnsFct = None
        self._outBdd = None
        self._primedVars = [None] * self.varsLength
        self._toPrimesPerm = None  # permutation
        self._rmvPrimesPerm = None  # permutation
        #index is the position of the variable inside the bitVector
        self.pntIndices = aiger.getLatchesIndicesList()  # int
        self.inputIndicies = aiger.getInputIndicesList()  # int
        self._nbOfLatches = aiger.getNbOfLatches()
        self._nbOfInputs = aiger.getNbOfInputs()
        self._nbOfAndGates = aiger.getNbOfAndGates()
        self._latches = aiger.getLatches()
        # vars mean the value from the aiger file
        self._inputVars = aiger.getInputVars()
        self._outputVar = aiger.getOutputVars()[0]  # must contain just one
        self._andGates = aiger.getAndGates()
        self._aig = aiger  # not used except for isVarNegated
        #indicies in the _vars array
        self._cInputIndices = aiger.getcInputIndices()  # int
        self._ucInputIndices = aiger.getucInputIndices()  # int
        self.refList = set()  # int use add not append
        self.derefList = []  # int
        self.andSet = aiger.andSet  # Map<Integer, List<Integer>>
        self.bddManager = bdd
        # in case the output bdd contains input
        #never create a new variable, because here we just have uncontrollable
        #inuts
        self.hasNewOutputVar = False
        #the + 1 is for newoutputvar
        nxtTimFctLength =  self._nbOfInputs + (self._nbOfLatches * 2)
        if self.hasNewOutputVar:
            nxtTimFctLength += 2
        self.nxtTimFct = self.bddManager.createDdArray(nxtTimFctLength)
        self.getBDDFromAigerFile()

    def isVarNegated(self, var):
        return self._aig.isVarNegated(var)

    # in case the output bdd contains input
    def addNewVariableForOutput(self):
        newOutputVar = self.bddManager.createVar()
        self._vars[self.varsLength - 1] = newOutputVar
        #if no variables were introduced
        if(len(self.pntIndices) > 0):
            highestIndex = self.pntIndices[len(self.pntIndices) - 1]
            self.pntIndices.append(highestIndex + 1)
        else:
            self.pntIndices.append(len(self.inputIndicies))
        self._nbOfLatches = self._nbOfLatches + 1
        self._latches.append(Latch(self.varsLength - 1, self._outputVar))
        self._outBdd = newOutputVar
        return newOutputVar

    def getInitialPoint(self):
        initialPointCube = self.bddManager.getTrue()
        for lch in self._latches:
            initialPointCube = self.bddManager.andTo(initialPointCube,
                 self.bddManager.not_no_deref(self._vars[lch.getLeftVar()]))
        return initialPointCube

    def getLatchesCube(self):
        cube = self.bddManager.getTrue()
        for lch in self._latches:
            cube = self.bddManager.andTo(cube, self._vars[lch.getLeftVar()])
        return cube

    def getPrimesCube(self):
        cube = self.bddManager.getTrue()
        for lch in self._latches:
            cube = self.bddManager.andTo(
                cube, self._primedVars[lch.getLeftVar()])
        return cube

    def getInputCube(self):
        cube = self.bddManager.getTrue()
        for inp in self._inputVars:
            cube = self.bddManager.andTo(cube, self._vars[inp])
        return cube

    def getInputNeg(self):
        cube = self.bddManager.getTrue()
        for inp in self._inputVars:
            cube = self.bddManager.andTo(cube,
            self.bddManager.not_(self._vars[inp]))
        return cube

    #here index means the var
    def getcInputIndices(self):
        return self._cInputIndices

    def getucInputIndices(self):
        return self._ucInputIndices

    def getUnContrInputCube(self):
        cube = self.bddManager.getTrue()
        for uinp in self._ucInputIndices:
            cube = self.bddManager.andTo(cube, self._vars[uinp])
        return cube

    def getUnContrInputCubeNeg(self):
        cube = self.bddManager.getTrue()
        for uinp in self._ucInputIndices:
            cube = self.bddManager.andTo(cube, self.bddManager.not_(self._vars[uinp]))
        return cube

    def getUnContrInputCubeTest(self, bitVec):
        cube = self.bddManager.getTrue()
        counter = 0
        print self._ucInputIndices
        for uinp in self._ucInputIndices:
            if(bitVec[counter] == '0'):
                cube = self.bddManager.andTo(cube,
                    self.bddManager.not_no_deref(self._vars[uinp]))
            else:
                cube = self.bddManager.andTo(cube, self._vars[uinp])
            counter = counter + 1
        return cube

    def getPntCubeTest(self, bitVec):
        cube = self.bddManager.getTrue()
        counter = 0
        for lch in self._latches:
            if(bitVec[counter] == '0'):
                cube = self.bddManager.andTo(cube,
                     self.bddManager.not_no_deref(self._vars[lch.getLeftVar()]))
            else:
                cube = self.bddManager.andTo(cube, self._vars[lch.getLeftVar()])
            counter = counter + 1
        return cube

    def getPrimeCubeTest(self, bitVec):
        cube = self.bddManager.getTrue()
        counter = 0
        for lch in self._latches:
            if(bitVec[counter] == '0'):
                cube = self.bddManager.andTo(cube,
                     self.bddManager.not_no_deref(self._primedVars[lch.getLeftVar()]))
            else:
                cube = self.bddManager.andTo(cube, self._primedVars[lch.getLeftVar()])
            counter = counter + 1
        return cube

    def getPrimeNeg(self):
        cube = self.bddManager.getTrue()
        for lch in self._latches:
            cube = self.bddManager.andTo(cube,
                 self.bddManager.not_no_deref(self._primedVars[lch.getLeftVar()]))
        return cube

    def getTransCubeTest(self, bitVec):
        cube = self.bddManager.getTrue()
        counter = 0
        for vr in self.bddManager.vars:
            if(bitVec[counter] == '0'):
                cube = self.bddManager.andTo(cube,
                     self.bddManager.not_no_deref(vr))
            else:
                cube = self.bddManager.andTo(cube, vr)
            counter = counter + 1
        return cube

    def getContrInputCube(self):
        cube = self.bddManager.getTrue()
        for cinp in self._cInputIndices:
            cube = self.bddManager.andTo(cube, self._vars[cinp])
        return cube

    def getOutputBddPrimed(self):
        outPrimed = self.bddManager.replace_no_deref(
            self._outBdd, self._toPrimesPerm)
        return outPrimed

    def getBDDManager(self):
        return self.bddManager  # BDDBase<bdd, Perm>

    def getVars(self):
        return self._vars

    def getTrnsFct(self):
        return self._trnsFct

    def setTrnsFct(self, trnsFct):
        self._trnsFct = trnsFct

    def getOutBdd(self):
        return self._outBdd

    def getPrimedVars(self):
        return self._primedVars

    def getToPrimesPerm(self):
        return self._toPrimesPerm

    def getRmvPrimesPerm(self):
        return self._rmvPrimesPerm

    def getPntIndices(self):
        return self.pntIndices

    def getInputIndicies(self):
        return self.inputIndicies

    def getLatches(self):
        return self._latches

    def getBDDFromAigerFile(self):
        varCounter = 0
        bddsTobeChecked = []  # int
        intNotTobeDerefd = []  # bdd
        #creating input variables
        for i in self._inputVars:
            newInput = self.bddManager.createVar()
            self._vars[i] = newInput
            intNotTobeDerefd.append(newInput)
            #as it has nothing to do with next time
            self.nxtTimFct[varCounter] = newInput
            varCounter += 1
        #creating states variables
        for lch in self._latches:
            newLatch = self.bddManager.createVar()
            self._vars[lch.getLeftVar()] = newLatch
            intNotTobeDerefd.append(newLatch)
            self.nxtTimFct[varCounter] = newLatch
            varCounter += 1
        #compute AND gates BDDs
        andGatesQueue = copy.deepcopy( self._andGates)
        while len(andGatesQueue) > 0:
            tempAg = andGatesQueue.pop(0)
            if tempAg is not None and\
            self._vars[tempAg.getAgVariable()] is None:
                ll = tempAg.getLeft()  # Literal
                rl = tempAg.getRight()  # Literal
                andBdd = None  # bdd
                if self._vars[ll.getVariable()] is not None and\
                 self._vars[rl.getVariable()] is not None:
                    left = self.bddManager.not_no_deref(  # bdd
                        self._vars[ll.getVariable()])\
                        if ll.isNegated() else self._vars[ll.getVariable()]
                    right = self.bddManager.not_no_deref(  # bdd
                        self._vars[rl.getVariable()])\
                        if rl.isNegated() else self._vars[rl.getVariable()]
                    andBdd = self.bddManager.and_no_deref(left, right)
                    ###if ll.isNegated():
                    ###    self.bddManager.deref(left)
                    ###if rl.isNegated():
                    ###    self.bddManager.deref(right)
                elif self._vars[ll.getVariable()] is not None and\
                rl.getLiteralValue() == 1:
                    andBdd = self.bddManager.not_no_deref(
                        self._vars[ll.getVariable()])\
                        if ll.isNegated() else self._vars[ll.getVariable()]
                elif rl.getLiteralValue() == 0:
                    andBdd = self.bddManager.getNotTrue()
                elif ll.getLiteralValue() == 1 and\
                self._vars[rl.getVariable()] is not None:
                    andBdd = self.bddManager.not_no_deref(
                        self._vars[rl.getVariable()])\
                        if rl.isNegated() else self._vars[rl.getVariable()]
                elif ll.getLiteralValue() == 0:
                    andBdd = self.bddManager.getNotTrue()
                elif ll.getLiteralValue() == 1 and rl.getLiteralValue() == 1:
                    andBdd = self.bddManager.getTrue()
                if andBdd is not None:
                    self._vars[tempAg.getAgVariable()] = andBdd
                    # bdd.printDot("Test\\"+tempAg.getAgVariable(), and);
                    bddsTobeChecked.append(tempAg.getAgVariable())
                else:
                    andGatesQueue.append(tempAg)

            ###bddsTobeChecked = self.garbageCollect(j, bddsTobeChecked)
        #if output contains input variable create new variable
        if(self.hasNewOutputVar):
            newOutputVar = self.addNewVariableForOutput()
            intNotTobeDerefd.append(newOutputVar)
            self.nxtTimFct[varCounter] = newOutputVar
            varCounter += 1
            print('new var was created')
        #initialise variables for Perm
        latchesVarsforPerm = [None] * self._nbOfLatches
        primedVarsforPerm = [None] * self._nbOfLatches
        permCounter = 0
        #initialise transition function
        self._trnsFct = self.bddManager.getTrue()
        #for each latch create a variable which is the prime variable
        #the left coud not be negated but right could be, and it could be 0 or 1
        #tStart = int(round(time.time()))
        #leftVar = None
        #print("start extracting latches " + str(len(self._latches)))
        #counter = 0
        for lch in self._latches:
            #start = int(round(time.time() * 1000))
            #create primedd var
            primeVar = self.bddManager.createVar()  # bdd
            intNotTobeDerefd.append(primeVar)
            #manage prime vars and Perm
            self._primedVars[lch.getLeftVar()] = primeVar
            latchesVarsforPerm[permCounter] = self._vars[lch.getLeftVar()]
            primedVarsforPerm[permCounter] = primeVar
            permCounter = permCounter + 1
            #end of "manage prime vars and Perm"
            #take into consideration if the righ var was 0 or 1
            if self._vars[lch.getRightVar()] is None:
                self._vars[lch.getRightVar()] = self.bddManager.getTrue()\
                if (lch.getRightVar() == 1) else self.bddManager.getNotTrue()

            self.nxtTimFct[varCounter] = self.bddManager.not_(
                self._vars[lch.getRightVar() - 1])\
            if self.isVarNegated(lch.getRightVar())\
            else self._vars[lch.getRightVar()]
            varCounter += 1
        self._toPrimesPerm = self.bddManager.createPermutation(
            latchesVarsforPerm, primedVarsforPerm)
        #replace primes py normal ones
        self._rmvPrimesPerm = self.bddManager.createPermutation(
            primedVarsforPerm, latchesVarsforPerm)
        #end of "manage prime vars and Perm"/
        #there must be one output in the aiger files we target
        if(not self.hasNewOutputVar):
            #print('no new var created')
            outputIndex = self._outputVar
            outputBDD = self.bddManager.not_(self._vars[outputIndex - 1])\
            if (outputIndex % 2 == 1) else self._vars[outputIndex]
            #the same as before just because we have no controllable inputs
            #so we quantify inputs
            self._outBdd = self.bddManager.exists_no_deref(outputBDD,
                self.getInputCube())
        #print self.bddManager.satAssignCount(self._trnsFct)


    def getInitBitVector(self):
        pntLength = len(self.pntIndices)
        bitVector = '0' * pntLength
        return bitVector

    def getInputBitVector(self, inpt):
        bitVector = ''
        tmp = self.bddManager.getMintermBitVector(inpt)  # int[]
        for i in self.inputIndicies:
            bitVector = bitVector + tmp[i]
        return bitVector

    def getPntBitVector(self, pnt):
        bitVector = ''
        tmp = self.bddManager.getMintermBitVector(pnt)
        for i in self.pntIndices:
            bitVector = bitVector + tmp[i]
        return bitVector

    def getFullBitVector(self, pnt):
        bitVector = self.bddManager.getMintermBitVector(pnt)
        return bitVector

    #for testing purposes
    def getAllBitVectors(self, bdd):
        lst = self.getPointsListForDraw(bdd)
        newList = []
        for bddPoint in lst:
            pntV = self.getFullBitVector(bddPoint)
            newList.append(pntV)
        return newList

    def getPointsListForDraw(self, bdd):  # bdd, List<int>
        temp = None  # bdd
        pointsList = []  # List<bdd>
        #returns the number of points
        #if nbOfPoints == 0.0:
        #    return pointsList
        counter = 0
        satAssignment = None
        while ((bdd != self.bddManager.getFalse()) and
        (bdd != self.bddManager.getNotOne())):
            satAssignment = self.bddManager.getMintermBDD(bdd)
            #print self.getPntBitVector(satAssignment)
            pointsList.append(satAssignment)
            temp = self.bddManager.not_no_deref(satAssignment)
            bdd = self.bddManager.and_no_deref(bdd, temp)
            self.bddManager.deref(temp)
            #print counter
            counter = counter + 1
        return pointsList


    def getPrimedVersion(self, bdd):
        bddPrimed = self.bddManager.replace(
                    bdd, (self._toPrimesPerm))
        return bddPrimed

    #in each iteration just consider unvisited states
    #basically what this model checker is doing is it returns
    #self.levels which contains error paths fro initial state until the
    #shallower reachable errors
    def ModelCheck(self):
        try:
            #print self._vars
            initCube = self.getInitialPoint()
            inputCube = self.getInputCube()
            #we start from the set of unsafe states
            visitedStates = self._outBdd
            tobeCheckedStates = self._outBdd
            initCheck = None  # to check if initial point is in a level
            while True:
                # compute newTmp
                currentLevelPrimed = self.getPrimedVersion(
                    tobeCheckedStates)
                # I called it previous level as we are going backword
                #print 'hi'
                temp = currentLevelPrimed.VectorCompose(self.nxtTimFct)
                #print 'temp'
                previousLevel = self.bddManager.exists(
                    temp,
                    inputCube)
                # check if initial point is in the computed level
                initCheck = self.bddManager.and_no_deref(previousLevel, initCube)
                if not initCheck == self.bddManager.getNotTrue():
                    self.bddManager.deref(initCheck)
                    return False
                self.bddManager.deref(initCheck)
                previousLevel = self.bddManager.and_no_deref(previousLevel,
                     self.bddManager.not_no_deref(visitedStates))
                # check if fixed point
                if(previousLevel == self.bddManager.getNotTrue()):
                    return True
                visitedStates = self.bddManager.or_no_deref(visitedStates,
                    previousLevel)
                tobeCheckedStates = previousLevel
                # end of check if fixed point

        except Exception as ex:
            print(ex)
            return False


    def ValidateWinningRegion(self, winRegAigFile):
        winRegion = self.ReadWinningRegionDynamic(winRegAigFile)
        #check if init is in winRegion
        initCube = self.getInitialPoint()
        initCheck = self.bddManager.and_no_deref(winRegion, initCube)
        if initCheck == self.bddManager.getNotTrue():
            return 'False:initial state is not in the winning region!'
        #end of check if init is in winRegion
        #check if wining region contains Errors
        errorCheck = self.bddManager.and_no_deref(winRegion, self._outBdd)
        if not errorCheck == self.bddManager.getNotTrue():
            return 'False:Winning region contains an error state!'
        #end of check if wining region contains Errors
        #get an image to check if it is a fixed point
        #preimage(!W) & W = 0
        loosingRegion = self.bddManager.not_no_deref(winRegion)
        preimage = loosingRegion.VectorCompose(self.nxtTimFct)
        fixedPntCheck = self.bddManager.and_no_deref(preimage, winRegion)
        if(fixedPntCheck == self.bddManager.getNotTrue()):
            return 'True'
        print 'False:The winning region is not a fixed point!!'
        #end of get an image to check if it is a fixed point

    #this is not used anymore to give users freedom when choosing
    #indices for state variable, they dont have to stick to indices
    #used in the strategy
    def ReadWinningRegion(self,winRegAigFile):
        #compute the andgates
        wrInputs = winRegAigFile.getInputVars()
        forRange = list(range(winRegAigFile.getNbOfAndGates()))
        wrAndGates = winRegAigFile.getAndGates()
        wrVars = dict()
        for andGate in wrAndGates:
            wrVars[andGate.getAgVariable()] = None
        for inpt in wrInputs:
            wrVars[inpt] = self._vars[inpt]
        for j in forRange:
            tempAg = wrAndGates[j]
            if tempAg is not None and\
            wrVars[tempAg.getAgVariable()] is None:
                ll = tempAg.getLeft()  # Literal
                rl = tempAg.getRight()  # Literal
                #print wrVars
                andBdd = None  # bdd
                #print ll.getVariable()
                if wrVars[ll.getVariable()] is not None and\
                 wrVars[rl.getVariable()] is not None:
                    left = self.bddManager.not_no_deref(  # bdd
                        wrVars[ll.getVariable()])\
                        if ll.isNegated() else wrVars[ll.getVariable()]
                    right = self.bddManager.not_no_deref(  # bdd
                        wrVars[rl.getVariable()])\
                        if rl.isNegated() else wrVars[rl.getVariable()]
                    andBdd = self.bddManager.and_no_deref(left, right)
                elif wrVars[ll.getVariable()] is not None and\
                rl.getLiteralValue() == 1:
                    andBdd = self.bddManager.not_no_deref(
                        wrVars[ll.getVariable()])\
                        if ll.isNegated() else wrVars[ll.getVariable()]
                elif wrVars[ll.getVariable()] is not None and\
                rl.getLiteralValue() == 0:
                    andBdd = self.bddManager.getNotTrue()
                elif ll.getLiteralValue() == 1 and\
                wrVars[rl.getVariable()] is not None:
                    andBdd = self.bddManager.not_no_deref(
                        wrVars[rl.getVariable()])\
                        if rl.isNegated() else wrVars[rl.getVariable()]
                elif ll.getLiteralValue() == 0 and\
                wrVars[rl.getVariable()] is not None:
                    andBdd = self.bddManager.getNotTrue()
                if andBdd is not None:
                    wrVars[tempAg.getAgVariable()] = andBdd
        outputVar = winRegAigFile.getOutputVars()[0]
        isNegated = outputVar % 2
        wrBDD = self.bddManager.not_no_deref(wrVars[outputVar - 1])\
        if isNegated == 1 else wrVars[outputVar]
        return wrBDD


    def ReadWinningRegionDynamic(self,winRegAigFile):
        #compute the andgates
        lchIndcs = self._latches
        wrInputs = winRegAigFile.getInputVars()
        #input(winning region) Latches(strategy)Correspondance
        corr = dict()
        indx = 0
        for inpt in wrInputs:
            corr[inpt] = lchIndcs[indx].getLeftVar()
            indx += 1
        #end of input(winning region) Latches(strategy)Correspondance
        forRange = list(range(winRegAigFile.getNbOfAndGates()))
        wrAndGates = winRegAigFile.getAndGates()
        wrVars = dict()
        for andGate in wrAndGates:
            wrVars[andGate.getAgVariable()] = None
        for inpt in wrInputs:
            wrVars[inpt] = self._vars[corr[inpt]]
        for j in forRange:
            tempAg = wrAndGates[j]
            if tempAg is not None and\
            wrVars[tempAg.getAgVariable()] is None:
                ll = tempAg.getLeft()  # Literal
                rl = tempAg.getRight()  # Literal
                #print wrVars
                andBdd = None  # bdd
                #print ll.getVariable()
                if wrVars[ll.getVariable()] is not None and\
                 wrVars[rl.getVariable()] is not None:
                    left = self.bddManager.not_no_deref(  # bdd
                        wrVars[ll.getVariable()])\
                        if ll.isNegated() else wrVars[ll.getVariable()]
                    right = self.bddManager.not_no_deref(  # bdd
                        wrVars[rl.getVariable()])\
                        if rl.isNegated() else wrVars[rl.getVariable()]
                    andBdd = self.bddManager.and_no_deref(left, right)
                elif wrVars[ll.getVariable()] is not None and\
                rl.getLiteralValue() == 1:
                    andBdd = self.bddManager.not_no_deref(
                        wrVars[ll.getVariable()])\
                        if ll.isNegated() else wrVars[ll.getVariable()]
                elif wrVars[ll.getVariable()] is not None and\
                rl.getLiteralValue() == 0:
                    andBdd = self.bddManager.getNotTrue()
                elif ll.getLiteralValue() == 1 and\
                wrVars[rl.getVariable()] is not None:
                    andBdd = self.bddManager.not_no_deref(
                        wrVars[rl.getVariable()])\
                        if rl.isNegated() else wrVars[rl.getVariable()]
                elif ll.getLiteralValue() == 0 and\
                wrVars[rl.getVariable()] is not None:
                    andBdd = self.bddManager.getNotTrue()
                if andBdd is not None:
                    wrVars[tempAg.getAgVariable()] = andBdd
        outputVar = winRegAigFile.getOutputVars()[0]
        if(outputVar == 1):
            return self.bddManager.getTrue()
        isNegated = outputVar % 2
        wrBDD = self.bddManager.not_no_deref(wrVars[outputVar - 1])\
        if isNegated == 1 else wrVars[outputVar]
        return wrBDD


#    def readTheStrategy(self):
#        pass