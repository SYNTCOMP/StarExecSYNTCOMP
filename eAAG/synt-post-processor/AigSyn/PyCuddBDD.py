#!/usr/bin/env python
from BDDBase import BDDBase
import sys
sys.path.append("../pycudd")
import pycudd
from subprocess import call
import time


class PyCuddBDD(BDDBase):

    def __init__(self):
        self._bddManager = pycudd.DdManager()
        self._bddManager.SetDefault()
        self._bddManager.AutodynEnable(4)
        self.varsNum = 0
        self.vars = []

    def getTrue(self):
        return self._bddManager.ReadOne()

    def getNotOne(self):
        return ~(self._bddManager.ReadOne())

    def getNotTrue(self):
        return ~(self.getTrue())

    def getFalse(self):
        return self._bddManager.ReadZero()

    def createVar(self):
        newVar = self._bddManager.IthVar(self.varsNum)
        self.vars.append(newVar)
        self.varsNum += 1
        return newVar

    def draw(self, bddName, bdd):
        bdd.DumpDot()
        time.sleep(3)
        call(["dot", "-Tjpg", "out.dot", "-o", bddName + ".jpg"])

    def ref(self, bdd):
        #self._bddManager.ref(bdd)
        pass

    #no need for the deref as python does that when destroying the objects
    #that holds the bdd
    def deref(self, bdd):
        #if(bdd != self.getTrue()):
        #    self._bddManager.KillNode(bdd.__int__())
        pass

    def andTo(self, left, right):
        temp = left.And(right)
        self.deref(left)
        return temp

    def andAll(self, bddList):
        bddAll = self.getTrue()
        for bdd in bddList:
            bddAll = self.andTo(bddAll, bdd)
        return bddAll

    def and_no_deref(self, left, right):
        return left & right

    def and_(self, left, right):
        temp = left & right
        self.deref(left)
        self.deref(right)
        return temp

    def orTo(self, left, right):
        temp = left | right
        self.deref(left)
        return temp

    def or_no_deref(self, left, right):
        return left | right

    def or_(self, left, right):
        temp = left | right
        self.deref(left)
        self.deref(right)
        return temp

    def not_no_deref(self, bdd):
        return ~bdd

    def not_(self, bdd):
        temp = ~bdd
        self.deref(bdd)
        return temp

    def biimpTo(self, left, right):
        temp = (~left | right) & (~right | left)
        self.deref(left)
        return temp

    def biimp_no_deref(self, left, right):
        if(left == self.getFalse() or
            left == self.getNotTrue()):
                return self.not_no_deref(right)
        temp2 = (~right | left)
        temp1 = (~left | right)
        temp = temp1 & temp2
        return temp

    def biimp(self, left, right):
        temp = (~left | right) & (~right | left)
        self.deref(left)
        self.deref(right)
        return temp

    def relProduct_no_deref(self, leftBDD, rightBDD, cube):
        return leftBDD.AndAbstract(rightBDD, cube)

    def relProductTo(self, leftBDD, rightBDD, cube):
        temp = leftBDD.AndAbstract(rightBDD, cube)
        self.deref(leftBDD)
        return temp

    def relProduct(self, leftBDD, rightBDD, cube):
        temp = leftBDD.AndAbstract(rightBDD, cube)
        self.deref(leftBDD)
        self.deref(rightBDD)
        return temp

    def exists_no_deref(self, bdd, cube):
        return bdd.ExistAbstract(cube)

    def exists(self, bdd, cube):
        temp = bdd.ExistAbstract(cube)
        self.deref(bdd)
        return temp

    def forAll_no_deref(self, bdd, cube):
        return bdd.UnivAbstract(cube)

    def forAll(self, bdd, cube):
        temp = bdd.UnivAbstract(cube)
        self.deref(bdd)
        return temp

    def createDdArray(self, length):
        return pycudd.DdArray(length)

    def createPermutation(self, from_, to):
        length = len(from_)
        fromArray = pycudd.DdArray(length)
        toArray = pycudd.DdArray(length)
        i = 0
        for var in from_:
            fromArray[i] = var
            i = i + 1
        i = 0
        for var in to:
            toArray[i] = var
            i = i + 1
        return [fromArray, toArray, length]

    def replace_no_deref(self, bdd, perm):
        #[fromArray, toArray, length]
        return bdd.SwapVariables(perm[0], perm[1], perm[2])

    def replace(self, bdd, perm):
        temp = bdd.SwapVariables(perm[0], perm[1], perm[2])
        self.deref(bdd)
        return temp

    def getSatAssign(self, bdd):
        pass

    def PrintMinterm(self, bdd):
        bdd.PrintMinterm()

    #returns a BDD that represents a single satisfying assignment
    def getMintermBDD(self, bdd):
        varArray = pycudd.DdArray(self.varsNum)
        i = 0
        for vr in self.vars:
            varArray[i] = vr
            i += 1
        return bdd.PickOneMinterm(varArray, self.varsNum)

    def getAllVarNegative(self):
        cube = self._bddManager.ReadOne()
        for vr in self.vars:
            cube = self.and_no_deref(cube, self.not_no_deref(vr))
        return cube

    def getMintermBitVector(self, bdd):
        satAsBDD = self.getMintermBDD(bdd)
        bitVec = ''
        for vr in self.vars:
            temp = satAsBDD & vr
            if(temp == satAsBDD):
                bitVec += '1'
            else:
                bitVec += '0'
            self.deref(temp)
        return bitVec

    def satAssignCount(self, bdd):
        return bdd.CountMinterm(self.varsNum)



#'typedef enum {
#    CUDD_REORDER_SAME,
#    CUDD_REORDER_NONE,
#    CUDD_REORDER_RANDOM,
#    CUDD_REORDER_RANDOM_PIVOT,
#    CUDD_REORDER_SIFT,
#    CUDD_REORDER_SIFT_CONVERGE,
#    CUDD_REORDER_SYMM_SIFT,
#    CUDD_REORDER_SYMM_SIFT_CONV,
#    CUDD_REORDER_WINDOW2,
#    CUDD_REORDER_WINDOW3,
#    CUDD_REORDER_WINDOW4,
#    CUDD_REORDER_WINDOW2_CONV,
#    CUDD_REORDER_WINDOW3_CONV,
#    CUDD_REORDER_WINDOW4_CONV,
#    CUDD_REORDER_GROUP_SIFT,
#    CUDD_REORDER_GROUP_SIFT_CONV,
#    CUDD_REORDER_ANNEALING,
#    CUDD_REORDER_GENETIC,
#    CUDD_REORDER_LINEAR,
#    CUDD_REORDER_LINEAR_CONVERGE,
#    CUDD_REORDER_LAZY_SIFT,
#    CUDD_REORDER_EXACT
#} Cudd_ReorderingType;
