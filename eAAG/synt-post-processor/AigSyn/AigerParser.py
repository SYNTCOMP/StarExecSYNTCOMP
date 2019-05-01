#!/usr/bin/env python
from FileHelper import FileHelper
from collections import deque

class Literal(object):

    def __init__(self, literal, isInput, isOutput, isAnd):
            self._isInput = isInput
            self._isOutput = isOutput
            self._literalValue = literal
            self._isAnd = isAnd

    def isInput(self):
        return self._isInput

    def isOutput(self):
        return self._isOutput

    def isAndGate(self):
        return self._isAnd

    def getLiteralValue(self):
        return self._literalValue

    #
    # 	 * a literal is negated if it is an odd integer
    # 	 * @return a boolean
    #
    def isNegated(self):
        return self._literalValue % 2 == 1

    #
    # 	 * the variable without negation
    # 	 * @return the original variable
    #
    def getVariable(self):
        if self.isNegated():
            return self._literalValue - 1
        return self._literalValue

    def __str__(self):
        return "[" + str(self._literalValue) + "]"


class Latch(object):

    def __init__(self, left, right):
        self._left = left
        self._right = right

    def getLeftVar(self):
        return self._left

    def getRightVar(self):
        return self._right

    def __str__(self):
        return "[" + str(self._left) + " " + str(self._right) + "]"


class AndGate(object):

    def getAgVariable(self):
        return self._agVariable

    def getLeft(self):
        return self._left

    def getRight(self):
        return self._right

    def __init__(self, agVar, left, right):
        self._agVariable = agVar
        self._left = left
        self._right = right
#        self.inputs = []

    def __str__(self):
        if self._left is not None and self._right is not None:
            return "[" + str(self._agVariable)\
            + " " + str(self._left.getLiteralValue())\
            + " " + str(self._right.getLiteralValue()) + "]"
        return "[]"

#    def addInput(self, inp):
#        self.inputs.append(inp)

#    def addInputList(self, inp):
#        self.inputs.extend(inp)

    def getInputList(self):
        return self.inputs

    def hashCode(self):
        prime = 31
        result = 1
        result = prime * result + self._agVariable
        return result

    def equals(self, ob):
        if ob is None:
            return False
        if isinstance(ob, AndGate):
            return self._agVariable == (AndGate(ob)).getAgVariable()
        return False


class AigerFile(object):

    def __init__(self):
        self.inputVars = []
        self.outputVars = []
        self.andGatesVars = []
        self.latches = []
        self.andGates = []
        self.nbOfInputs = 0
        self.nbOfOutputs = 0
        self.nbOfLatches = 0
        self.nbOfAndGates = 0
        self.maxVarIndex = 0
        self._cInputIndices = []
        self._ucInputIndices = []
        self.andSet = None
        self.andGatesDict = None
        self.variables = []
        self.outputBddContainsInputVar = False

    def getAndSet(self):
        return self.andSet

    def getcInputIndices(self):
        return self._cInputIndices

    def getucInputIndices(self):
        return self._ucInputIndices

    def getInputVars(self):
        return self.inputVars

    def getOutputVars(self):
        return self.outputVars

    def getAndGatesVars(self):
        return self.andGatesVars

    def getLatches(self):
        return self.latches

    def getAndGates(self):
        return self.andGates

    def getNbOfInputs(self):
        return self.nbOfInputs

    def getNbOfOutputs(self):
        return self.nbOfOutputs

    def getNbOfLatches(self):
        return self.nbOfLatches

    def getNbOfAndGates(self):
        return self.nbOfAndGates

    def getMaxVarIndex(self):
        return self.maxVarIndex

    def isVarNegated(self, var):
        var = int(var)
        return var > 1 and var % 2 == 1

    def isInput(self, var):
        if self.isVarNegated(var):
            var = var - 1
        if self.inputVars is not None:
            if var in self.inputVars:
                return 1
        return 0

    def isVariable(self, var):
        if self.isVarNegated(var):
            var = var - 1
        if self.variables is not None:
            if var in self.variables:
                return 1
        return 0

    def isOutput(self, var):
        if self.isVarNegated(var):
            var = var - 1
        if self.outputVars is not None:
            if var in self.outputVars:
                return 1
        return 0

    def isAndGate(self, var):
        if self.isVarNegated(var):
            var = var - 1
        return self.andGates is not None\
         and self.andGates.contains(AndGate(var, None, None))

    def getAndGate(self, ag):
        for andGate in self.andGates:
            if andGate.getAgVariable() == ag:
                return andGate
        return None

    def getAndGate_Literal(self, lt):
        for andGate in self.andGates:
            if andGate.getAgVariable() == lt.getVariable():
                return andGate
        return None

    def __str__(self):
        return "[aag " + str(self.maxVarIndex) + " " + str(self.nbOfInputs)\
         + " " + str(self.nbOfLatches) + " " + str(self.nbOfOutputs)\
          + " " + str(self.nbOfAndGates) + "]"

    #
    # 	 * get the variable indices that are used for points (latches indices)
    # 	 * @return List<Integer>, list of latches indices
    #
    def getLatchesIndicesList(self):
        indicies = []
        stopIndex = self.nbOfInputs + self.nbOfLatches
        startIndex = self.nbOfInputs + 1
        i = startIndex
        while i <= stopIndex:
            indicies.append(i)
            i += 1
        return indicies

    #
    # 	 * get the variable indices that are used for inputs (latches indices)
    # 	 * @return List<Integer>, list of latches indices
    #
    def getInputIndicesList(self):
        indicies = []
        stopIndex = self.nbOfInputs - 1
        startIndex = 0
        i = startIndex
        while i <= stopIndex:
            indicies.append(i)
            i += 1
        return indicies

    def checkOutputForInput(self):
        outputVar = self.outputVars[0]
        if(outputVar % 2 == 1):
                outputVar = outputVar - 1
        if(outputVar == 1 or outputVar == 0):
            return False
        queue = deque([outputVar])
        while(len(queue) > 0):
            andValue = queue.popleft()
            literals = self.andGatesDict[andValue]
            if(literals[0].isInput() or literals[1].isInput()):
                return True
            else:
                if(literals[0].isAndGate()):
                    queue.append(literals[0].getVariable())
                if(literals[1].isAndGate()):
                    queue.append(literals[1].getVariable())
        return False


class AigerFileParser(object):

    def __init__(self, filePath):
        self._fileText = FileHelper.readAllLinesFromFile(filePath)
        self._fileLength = len(self._fileText)
        self.aigFile = AigerFile()

    #aiger header is the first line in aiger file
    #and has the following format: aag M I L O A
    #where aag stands for ascii AIG
    #The interpretation of the integers is as follows
    #M = maximum variable index
    #I = number of inputs=
    #number of latches
    #number of outputs
    #A = number of AND gates
    #the function set the class member
    def getAigerHeader(self):
        header = []
        if self._fileLength > 0:
            header = self._fileText[0].split(' ')
        if len(header) == 6:
            self.aigFile.maxVarIndex = int(header[1])
            self.aigFile.nbOfInputs = int(header[2])
            self.aigFile.nbOfLatches = int(header[3])
            self.aigFile.nbOfOutputs = int(header[4])
            self.aigFile.nbOfAndGates = int(header[5])

    #the inputs literals start directly after the header line
    #, which are represented as integers
    #where each integer is on a single line
    #@return List<Integer> set of inputs
    def getInputs(self):
        inputs = []
        if self.aigFile.nbOfInputs > 0:
            stopIndex = self.aigFile.nbOfInputs
            startIndex = 1
            if self._fileLength >= stopIndex:
                forRange = list(range(startIndex, stopIndex + 1))
                for i in forRange:
                    inputs.append(int(self._fileText[i].strip()))
            self.aigFile.inputVars = inputs

    #the Latches start directly after the inputs ,
    # where each latch is represented as 2 integers seperated
    #by a single white space where each latch is on a single line
    #@return List<Integer> set of outputs
    def getLatches(self):
        latches = []
        variables = []
        if self.aigFile.nbOfLatches > 0:
            stopIndex = self.aigFile.nbOfInputs + self.aigFile.nbOfLatches
            startIndex = 1 + self.aigFile.nbOfInputs
            if self._fileLength >= stopIndex:
                for1Range = list(range(startIndex, stopIndex + 1))
                for i in for1Range:
                    latchStr = self._fileText[i].strip().split(' ')
                    latches.append(
                    Latch(int(latchStr[0].strip()), int(latchStr[1].strip()))
                    )
                    variables.append(int(latchStr[0].strip()))
            self.aigFile.latches = latches
            self.aigFile.variables = variables

    #the outputs literals start directly after the latches ,
    #which are represented as integers
    #where each integer is on a single line
    #@return List<Integer> set of outputs
    def getOutputs(self):
        outputs = []
        if self.aigFile.nbOfOutputs > 0:
            stopIndex = self.aigFile.nbOfInputs + self.aigFile.nbOfOutputs\
            + self.aigFile.nbOfLatches
            startIndex = 1 + self.aigFile.nbOfInputs + self.aigFile.nbOfLatches
            for2Range = list(range(startIndex, stopIndex + 1))
            for i in for2Range:
                outputs.append(int(self._fileText[i].strip()))
            self.aigFile.outputVars = outputs

    #Get controllable and uncontrollable input indices
    def getcontAnduncontInputIndices(self):
        cInputIndices = []
        ucInputIndices = []
        startIndex = 1 + self.aigFile.nbOfInputs + self.aigFile.nbOfOutputs +\
        self.aigFile.nbOfLatches + self.aigFile.nbOfAndGates
        found = 0
        textSize = len(self._fileText)
        while startIndex < textSize:
            line = self._fileText[startIndex].strip()
            if line.startswith('i') and line[1].isdigit():
                found = 1
                array = line.split(' ')
                if 'controllable' in line.lower():
                    cInputIndices.append(
                        (int(array[0].replace('i', '')) + 1) * 2)
                else:
                    ucInputIndices.append(
                        (int(array[0].replace('i', '')) + 1) * 2)
            elif found:
                break
            startIndex += 1
        self.aigFile._cInputIndices = cInputIndices
        self.aigFile._ucInputIndices = ucInputIndices

    #the andGates start directly after the latches ,
    #where each andgate is represented as 3 integers seperated
    #by a single white space
    #the first integer is the result of the andgate and the remaining
    #2 integers are the inputs of the andgate
    #each and gate is on a seperate line
    #@return List<Integer> set of outputs
    def getAndGates(self):
        andGates = []
        andGatesDict = dict()
        if self.aigFile.nbOfAndGates > 0:
            stopIndex = self.aigFile.nbOfInputs + self.aigFile.nbOfOutputs +\
            self.aigFile.nbOfLatches + self.aigFile.nbOfAndGates
            startIndex = 1 + self.aigFile.nbOfInputs + self.aigFile.nbOfOutputs\
             + self.aigFile.nbOfLatches
            andGateStr = self._fileText[startIndex].strip().split(' ')
            startAnd = int(andGateStr[0])
            self.aigFile.andSet = dict()  # new HashMap<Integer, List<Integer>>
            if self._fileLength >= stopIndex:
                for3Range = list(range(startIndex, stopIndex + 1))
                for i in for3Range:
                    andGateStr = self._fileText[i].strip().split(' ')
                    andGateVar = int(andGateStr[0].strip())
                    self.aigFile.andSet[andGateVar] = []
                    left = int(andGateStr[1].strip())
                    ntngLeft = (left - 1) if (left % 2 == 1) else left
                    isInput = self.aigFile.isInput(left)
                    isOutput = self.aigFile.isOutput(left)
                    leftLiteral = Literal(left, isInput, isOutput,
                         (ntngLeft in andGatesDict))
                    right = int(andGateStr[2].strip())
                    ntngRight = (right - 1) if (right % 2 == 1) else right
                    isInput = self.aigFile.isInput(right)
                    isOutput = self.aigFile.isOutput(right)
                    rightLiteral = Literal(right, isInput, isOutput,
                     (ntngRight in andGatesDict))
                    andGate = AndGate(andGateVar, leftLiteral, rightLiteral)
                    #check if it contains input
                    #tobedone
                    #to be done
                    #end of check if it contains input
                    andGates.append(andGate)
                    andGatesDict[andGateVar] = (leftLiteral, rightLiteral)
                    #the below is to be used in garbage collection
                    if left >= startAnd:
                        left = (left - 1) if (left % 2 == 1) else left
                        indicesList = self.aigFile.andSet.get(left)
                        indicesList.append(i - startIndex)
                        self.aigFile.andSet[left] = indicesList
                    if right >= startAnd:
                        right = (right - 1) if (right % 2 == 1) else right
                        indicesList = self.aigFile.andSet.get(right)
                        indicesList.append(i - startIndex)
                        self.aigFile.andSet[right] = indicesList
                #remember that an and variable may be used in the latches
                #therefore add them to the andSet with a higher index than any
                for lch in self.aigFile.latches:
                    tempR = lch.getRightVar()
                    right = tempR - 1 if tempR % 2 == 1 else tempR
                    if right >= startAnd:
                        indicesList = self.aigFile.andSet.get(right)
                        indicesList.append(stopIndex + 1)
                        self.aigFile.andSet[right] = indicesList
                #or outputs
                for outp in self.aigFile.outputVars:
                    outp = outp - 1 if outp % 2 == 1 else outp
                    indicesList = self.aigFile.andSet.get(outp)
                    indicesList.append(stopIndex + 1)
                    self.aigFile.andSet[outp] = indicesList
        self.aigFile.andGates = andGates
        self.aigFile.andGatesDict = andGatesDict

    def parse(self):
        self.getAigerHeader()
        self.getInputs()
        self.getOutputs()
        self.getLatches()
        self.getAndGates()
        self.getcontAnduncontInputIndices()
        self.aigFile.outputBddContainsInputVar = self.aigFile.checkOutputForInput()
        return self.aigFile