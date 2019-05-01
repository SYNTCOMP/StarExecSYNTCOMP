#!/usr/bin/env python
from abc import abstractmethod


class BDDBase(object):

    @abstractmethod
    def getTrue(self):
        pass

    @abstractmethod
    def getFalse(self):
        pass

    @abstractmethod
    def createVar(self):
        pass

    @abstractmethod
    def andTo(self, left, right):
        pass

    @abstractmethod
    def andAll(self, right):
        pass

    @abstractmethod
    def and_no_deref(self, left, right):
        pass

    @abstractmethod
    def and_(self, left, right):
        pass

    @abstractmethod
    def orTo(self, left, right):
        pass

    @abstractmethod
    def or_no_deref(self, left, right):
        pass

    @abstractmethod
    def or_(self, left, right):
        pass

    @abstractmethod
    def not_no_deref(self, bdd):
        pass

    @abstractmethod
    def not_(self, bdd):
        pass

    @abstractmethod
    def biimpTo(self, left, right):
        pass

    @abstractmethod
    def biimp_no_deref(self, left, right):
        pass

    @abstractmethod
    def biimp(self, left, right):
        pass

    @abstractmethod
    def draw(self, bddName, bdd):
        pass

    @abstractmethod
    def relProduct_no_deref(self, leftBDD, rightBDD, cube):
        pass

    @abstractmethod
    def relProductTo(self, leftBDD, rightBDD, cube):
        pass

    @abstractmethod
    def relProduct(self, leftBDD, rightBDD, cube):
        pass

    @abstractmethod
    def createPermutation(self, from_, to):
        pass

    @abstractmethod
    def deref(self, bdd):
        pass

    @abstractmethod
    def ref(self, bdd):
        pass

    @abstractmethod
    def replace_no_deref(self, bdd, perm):
        pass

    @abstractmethod
    def replace(self, bdd, perm):
        pass

    @abstractmethod
    def exists_no_deref(self, bdd, cube):
        pass

    @abstractmethod
    def exists(self, bdd, cube):
        pass

    @abstractmethod
    def getSatAssign(self, bdd):
        pass

    @abstractmethod
    def satAssignCount(self, bdd):
        pass

